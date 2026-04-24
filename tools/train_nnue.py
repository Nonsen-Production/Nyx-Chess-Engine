#!/usr/bin/env python3
"""
NNUE Training Script for Nyx Chess Engine

Trains a 768 → 256 → 32 → 32 → 1 network on HCE-labeled positions
and exports quantized weights in the NYX1 format.

Usage:
  python3 tools/train_nnue.py --data train_data.bin --output nn-nyx.nnue --epochs 50

Prerequisites:
  pip install torch numpy
"""

import argparse
import struct
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader

# Network dimensions (must match nnue.h)
INPUT_SIZE = 768
ACC_SIZE = 256
H1_SIZE = 32
H2_SIZE = 32
FT_SCALE = 64
HIDDEN_SCALE = 64


class ChessDataset(Dataset):
    """Loads binary training data: 768 bytes features + 4 bytes eval + 1 byte stm."""

    def __init__(self, path):
        RECORD_SIZE = 768 + 4 + 1  # 773 bytes per position
        with open(path, "rb") as f:
            data = f.read()

        n = len(data) // RECORD_SIZE
        print(f"Loading {n} positions from {path}")

        self.features = np.zeros((n, INPUT_SIZE), dtype=np.float32)
        self.evals = np.zeros(n, dtype=np.float32)
        self.stm = np.zeros(n, dtype=np.int64)

        for i in range(n):
            offset = i * RECORD_SIZE
            feat = data[offset : offset + 768]
            eval_bytes = data[offset + 768 : offset + 772]
            stm_byte = data[offset + 772]

            self.features[i] = np.frombuffer(feat, dtype=np.uint8).astype(np.float32)
            self.evals[i] = struct.unpack("<i", eval_bytes)[0]
            self.stm[i] = stm_byte

        # Normalize evals to [-1, 1] range using sigmoid-like scaling
        # eval in centipawns, typical range [-2000, 2000]
        self.evals = np.tanh(self.evals / 400.0)

        print(f"Eval range: [{self.evals.min():.3f}, {self.evals.max():.3f}]")

    def __len__(self):
        return len(self.evals)

    def __getitem__(self, idx):
        return (
            torch.tensor(self.features[idx]),
            torch.tensor(self.evals[idx]),
            torch.tensor(self.stm[idx]),
        )


class NyxNNUE(nn.Module):
    """Nyx NNUE architecture: 768 → 256 → 32 → 32 → 1."""

    def __init__(self):
        super().__init__()
        # Feature transform (accumulator)
        self.ft = nn.Linear(INPUT_SIZE, ACC_SIZE)
        # Hidden layers
        self.hidden1 = nn.Linear(ACC_SIZE * 2, H1_SIZE)
        self.hidden2 = nn.Linear(H1_SIZE, H2_SIZE)
        self.output = nn.Linear(H2_SIZE, 1)

    def forward(self, features, stm):
        # The feature vector is from white's perspective
        # For the accumulator, we compute both perspectives
        # White perspective = features as-is
        # Black perspective = swap friendly/enemy features and mirror squares

        # Simple approach: use the same features for both perspectives
        # (proper mirroring would be done in data generation)
        white_acc = torch.clamp(self.ft(features), 0, 1)

        # For black, mirror the feature indices
        # Swap color blocks (0-383 ↔ 384-767) and mirror squares
        black_features = torch.zeros_like(features)
        for pt in range(6):
            for sq in range(64):
                mirror_sq = (7 - sq // 8) * 8 + sq % 8
                # White piece on sq → from black's POV: enemy piece on mirror_sq
                w_idx = pt * 64 + sq
                b_idx = 384 + pt * 64 + mirror_sq
                black_features[:, b_idx] = features[:, w_idx]
                # Black piece on sq → from black's POV: friendly piece on mirror_sq
                w_idx2 = 384 + pt * 64 + sq
                b_idx2 = pt * 64 + mirror_sq
                black_features[:, b_idx2] = features[:, w_idx2]

        black_acc = torch.clamp(self.ft(black_features), 0, 1)

        # Concatenate: STM first, then other
        stm_expanded = stm.unsqueeze(1)  # [B, 1]
        stm_acc = torch.where(stm_expanded == 0, white_acc, black_acc)
        other_acc = torch.where(stm_expanded == 0, black_acc, white_acc)
        concat = torch.cat([stm_acc, other_acc], dim=1)

        # Forward through hidden layers
        h1 = torch.clamp(self.hidden1(concat), 0, 1)
        h2 = torch.clamp(self.hidden2(h1), 0, 1)
        out = self.output(h2)

        return out.squeeze(1)  # Output is tanh-scaled eval


def quantize_and_export(model, output_path):
    """Export model weights in NYX1 binary format with quantization."""
    with open(output_path, "wb") as f:
        # Magic
        f.write(b"NYX1")

        # Feature transform weights: [768][256] int16_t, scale = FT_SCALE
        ft_w = model.ft.weight.data.T.numpy()  # [768, 256]
        ft_w_q = np.clip(ft_w * FT_SCALE, -32768, 32767).astype(np.int16)
        f.write(ft_w_q.tobytes())

        # Feature transform biases: [256] int16_t
        ft_b = model.ft.bias.data.numpy()
        ft_b_q = np.clip(ft_b * FT_SCALE, -32768, 32767).astype(np.int16)
        f.write(ft_b_q.tobytes())

        # Hidden1 weights: [512][32] int8_t, scale = HIDDEN_SCALE
        h1_w = model.hidden1.weight.data.T.numpy()  # [512, 32]
        h1_w_q = np.clip(h1_w * HIDDEN_SCALE, -128, 127).astype(np.int8)
        f.write(h1_w_q.tobytes())

        # Hidden1 biases: [32] int32_t
        h1_b = model.hidden1.bias.data.numpy()
        h1_b_q = np.clip(h1_b * FT_SCALE * HIDDEN_SCALE, -(2**31), 2**31 - 1).astype(
            np.int32
        )
        f.write(h1_b_q.tobytes())

        # Hidden2 weights: [32][32] int8_t
        h2_w = model.hidden2.weight.data.T.numpy()
        h2_w_q = np.clip(h2_w * HIDDEN_SCALE, -128, 127).astype(np.int8)
        f.write(h2_w_q.tobytes())

        # Hidden2 biases: [32] int32_t
        h2_b = model.hidden2.bias.data.numpy()
        h2_b_q = np.clip(h2_b * HIDDEN_SCALE * HIDDEN_SCALE, -(2**31), 2**31 - 1).astype(
            np.int32
        )
        f.write(h2_b_q.tobytes())

        # Output weights: [32] int8_t
        out_w = model.output.weight.data.squeeze().numpy()
        out_w_q = np.clip(out_w * HIDDEN_SCALE, -128, 127).astype(np.int8)
        f.write(out_w_q.tobytes())

        # Output bias: int32_t
        out_b = model.output.bias.data.item()
        out_b_q = np.int32(np.clip(out_b * HIDDEN_SCALE * HIDDEN_SCALE, -(2**31), 2**31 - 1))
        f.write(out_b_q.tobytes())

    print(f"Exported quantized weights to {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Train Nyx NNUE")
    parser.add_argument("--data", required=True, help="Training data file")
    parser.add_argument("--output", default="nn-nyx.nnue", help="Output .nnue file")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=4096)
    parser.add_argument("--lr", type=float, default=0.001)
    args = parser.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Using device: {device}")

    dataset = ChessDataset(args.data)
    loader = DataLoader(dataset, batch_size=args.batch_size, shuffle=True, num_workers=0)

    model = NyxNNUE().to(device)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    criterion = nn.MSELoss()

    best_loss = float("inf")
    for epoch in range(args.epochs):
        total_loss = 0.0
        batches = 0

        for features, evals, stm in loader:
            features = features.to(device)
            evals = evals.to(device)
            stm = stm.to(device)

            optimizer.zero_grad()
            pred = model(features, stm)
            loss = criterion(pred, evals)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            batches += 1

        avg_loss = total_loss / max(batches, 1)
        print(f"Epoch {epoch + 1}/{args.epochs}  Loss: {avg_loss:.6f}")

        if avg_loss < best_loss:
            best_loss = avg_loss
            quantize_and_export(model, args.output)
            print(f"  → Saved best model (loss={best_loss:.6f})")

    print(f"\nTraining complete. Best loss: {best_loss:.6f}")
    print(f"NNUE file: {args.output}")


if __name__ == "__main__":
    main()
