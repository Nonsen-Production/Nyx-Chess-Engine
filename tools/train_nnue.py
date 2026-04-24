#!/usr/bin/env python3
"""
NNUE Training Script for Nyx Chess Engine (v2)

Trains a 768 → 256 → 32 → 32 → 1 network using:
  - Search-based eval labels (centipawns)
  - Game result (WDL) blending for better convergence
  - Proper perspective handling

Usage:
  python3 tools/train_nnue.py --data train_data.bin --output nn-nyx.nnue --epochs 100

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


def sigmoid(x, scale=400.0):
    """Convert centipawn eval to [0, 1] win probability."""
    return 1.0 / (1.0 + np.exp(-x / scale))


class ChessDataset(Dataset):
    """Loads binary training data: 768 features + 4 eval + 1 stm + 1 result = 774 bytes."""

    def __init__(self, path):
        RECORD_SIZE = 774  # Updated format with game result
        with open(path, "rb") as f:
            data = f.read()

        n = len(data) // RECORD_SIZE
        remainder = len(data) % RECORD_SIZE

        # Try old format (773 bytes) if new format doesn't divide evenly
        if remainder != 0:
            RECORD_SIZE = 773
            n = len(data) // RECORD_SIZE
            has_result = False
            print(f"Detected old format (773 bytes/record)")
        else:
            has_result = True
            print(f"Detected new format (774 bytes/record, with game result)")

        print(f"Loading {n} positions from {path}")

        self.features = np.zeros((n, INPUT_SIZE), dtype=np.float32)
        self.targets = np.zeros(n, dtype=np.float32)
        self.stm = np.zeros(n, dtype=np.int64)

        WDL_WEIGHT = 0.5  # How much to blend game result vs eval

        for i in range(n):
            offset = i * RECORD_SIZE
            feat = data[offset : offset + 768]
            eval_bytes = data[offset + 768 : offset + 772]
            stm_byte = data[offset + 772]

            self.features[i] = np.frombuffer(feat, dtype=np.uint8).astype(np.float32)
            eval_cp = struct.unpack("<i", eval_bytes)[0]
            self.stm[i] = stm_byte

            # Convert eval to win probability
            eval_prob = sigmoid(eval_cp)

            if has_result:
                result_byte = data[offset + 773]
                # result: 0=black win, 1=draw, 2=white win → map to [0, 0.5, 1]
                result_prob = result_byte / 2.0
                # Blend eval and game result
                self.targets[i] = (1.0 - WDL_WEIGHT) * eval_prob + WDL_WEIGHT * result_prob
            else:
                self.targets[i] = eval_prob

        print(f"Target range: [{self.targets.min():.4f}, {self.targets.max():.4f}]")
        print(f"Mean target: {self.targets.mean():.4f}")

    def __len__(self):
        return len(self.targets)

    def __getitem__(self, idx):
        return (
            torch.tensor(self.features[idx]),
            torch.tensor(self.targets[idx]),
            torch.tensor(self.stm[idx]),
        )


class NyxNNUE(nn.Module):
    """Nyx NNUE: 768 → 256 → 32 → 32 → 1 with perspective."""

    def __init__(self):
        super().__init__()
        self.ft = nn.Linear(INPUT_SIZE, ACC_SIZE)
        self.hidden1 = nn.Linear(ACC_SIZE * 2, H1_SIZE)
        self.hidden2 = nn.Linear(H1_SIZE, H2_SIZE)
        self.output = nn.Linear(H2_SIZE, 1)

        # Initialize with small weights for stable training
        nn.init.kaiming_normal_(self.ft.weight, nonlinearity="relu")
        nn.init.kaiming_normal_(self.hidden1.weight, nonlinearity="relu")
        nn.init.kaiming_normal_(self.hidden2.weight, nonlinearity="relu")
        nn.init.xavier_normal_(self.output.weight)

    def forward(self, features, stm):
        # Compute white perspective accumulator
        white_acc = torch.clamp(self.ft(features), 0, 1)

        # Black perspective: swap colors (0-383 ↔ 384-767) and mirror squares
        black_features = torch.zeros_like(features)
        for pt in range(6):
            for sq in range(64):
                mirror_sq = (7 - sq // 8) * 8 + sq % 8
                w_idx = pt * 64 + sq
                b_idx = 384 + pt * 64 + mirror_sq
                black_features[:, b_idx] = features[:, w_idx]
                w_idx2 = 384 + pt * 64 + sq
                b_idx2 = pt * 64 + mirror_sq
                black_features[:, b_idx2] = features[:, w_idx2]

        black_acc = torch.clamp(self.ft(black_features), 0, 1)

        # Concatenate: side-to-move first
        stm_expanded = stm.unsqueeze(1)
        stm_acc = torch.where(stm_expanded == 0, white_acc, black_acc)
        other_acc = torch.where(stm_expanded == 0, black_acc, white_acc)
        concat = torch.cat([stm_acc, other_acc], dim=1)

        h1 = torch.clamp(self.hidden1(concat), 0, 1)
        h2 = torch.clamp(self.hidden2(h1), 0, 1)
        out = torch.sigmoid(self.output(h2))  # Output is win probability [0, 1]

        return out.squeeze(1)


class NyxNNUEFast(nn.Module):
    """Faster version that pre-computes the mirror mapping."""

    def __init__(self):
        super().__init__()
        self.ft = nn.Linear(INPUT_SIZE, ACC_SIZE)
        self.hidden1 = nn.Linear(ACC_SIZE * 2, H1_SIZE)
        self.hidden2 = nn.Linear(H1_SIZE, H2_SIZE)
        self.output = nn.Linear(H2_SIZE, 1)

        # Pre-compute mirror index mapping
        self.register_buffer("mirror_idx", self._build_mirror_map())

        nn.init.kaiming_normal_(self.ft.weight, nonlinearity="relu")
        nn.init.kaiming_normal_(self.hidden1.weight, nonlinearity="relu")
        nn.init.kaiming_normal_(self.hidden2.weight, nonlinearity="relu")
        nn.init.xavier_normal_(self.output.weight)

    @staticmethod
    def _build_mirror_map():
        idx = torch.zeros(768, dtype=torch.long)
        for pt in range(6):
            for sq in range(64):
                mirror_sq = (7 - sq // 8) * 8 + sq % 8
                # White friendly → black enemy
                idx[pt * 64 + sq] = 384 + pt * 64 + mirror_sq
                # White enemy → black friendly
                idx[384 + pt * 64 + sq] = pt * 64 + mirror_sq
        return idx

    def forward(self, features, stm):
        white_acc = torch.clamp(self.ft(features), 0, 1)

        # Fast mirror using pre-computed indices
        black_features = features[:, self.mirror_idx]
        black_acc = torch.clamp(self.ft(black_features), 0, 1)

        stm_expanded = stm.unsqueeze(1)
        stm_acc = torch.where(stm_expanded == 0, white_acc, black_acc)
        other_acc = torch.where(stm_expanded == 0, black_acc, white_acc)
        concat = torch.cat([stm_acc, other_acc], dim=1)

        h1 = torch.clamp(self.hidden1(concat), 0, 1)
        h2 = torch.clamp(self.hidden2(h1), 0, 1)
        out = torch.sigmoid(self.output(h2))
        return out.squeeze(1)


def quantize_and_export(model, output_path):
    """Export model weights in NYX1 binary format with quantization."""
    # Handle both model types
    ft = model.ft if hasattr(model, "ft") else model.ft

    with open(output_path, "wb") as f:
        f.write(b"NYX1")

        # Feature transform: [768][256] int16_t, scale = FT_SCALE
        ft_w = model.ft.weight.data.T.cpu().numpy()  # [768, 256]
        ft_w_q = np.clip(ft_w * FT_SCALE, -32768, 32767).astype(np.int16)
        f.write(ft_w_q.tobytes())

        ft_b = model.ft.bias.data.cpu().numpy()
        ft_b_q = np.clip(ft_b * FT_SCALE, -32768, 32767).astype(np.int16)
        f.write(ft_b_q.tobytes())

        # Hidden1: [512][32] int8_t
        h1_w = model.hidden1.weight.data.T.cpu().numpy()
        h1_w_q = np.clip(h1_w * HIDDEN_SCALE, -128, 127).astype(np.int8)
        f.write(h1_w_q.tobytes())

        h1_b = model.hidden1.bias.data.cpu().numpy()
        h1_b_q = np.clip(h1_b * FT_SCALE * HIDDEN_SCALE, -(2**31), 2**31 - 1).astype(np.int32)
        f.write(h1_b_q.tobytes())

        # Hidden2: [32][32] int8_t
        h2_w = model.hidden2.weight.data.T.cpu().numpy()
        h2_w_q = np.clip(h2_w * HIDDEN_SCALE, -128, 127).astype(np.int8)
        f.write(h2_w_q.tobytes())

        h2_b = model.hidden2.bias.data.cpu().numpy()
        h2_b_q = np.clip(h2_b * HIDDEN_SCALE * HIDDEN_SCALE, -(2**31), 2**31 - 1).astype(np.int32)
        f.write(h2_b_q.tobytes())

        # Output: [32] int8_t
        out_w = model.output.weight.data.squeeze().cpu().numpy()
        out_w_q = np.clip(out_w * HIDDEN_SCALE, -128, 127).astype(np.int8)
        f.write(out_w_q.tobytes())

        out_b = model.output.bias.data.item()
        out_b_q = np.int32(np.clip(out_b * HIDDEN_SCALE * HIDDEN_SCALE, -(2**31), 2**31 - 1))
        f.write(out_b_q.tobytes())

    print(f"Exported quantized weights to {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Train Nyx NNUE")
    parser.add_argument("--data", required=True, help="Training data binary file")
    parser.add_argument("--output", default="nn-nyx.nnue", help="Output .nnue file")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch-size", type=int, default=4096)
    parser.add_argument("--lr", type=float, default=0.001)
    parser.add_argument("--val-split", type=float, default=0.1, help="Validation split ratio")
    args = parser.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else
                          "mps" if torch.backends.mps.is_available() else "cpu")
    print(f"Using device: {device}")

    dataset = ChessDataset(args.data)

    # Train/val split
    n = len(dataset)
    val_n = int(n * args.val_split)
    train_n = n - val_n
    train_set, val_set = torch.utils.data.random_split(dataset, [train_n, val_n])

    train_loader = DataLoader(train_set, batch_size=args.batch_size, shuffle=True, num_workers=0)
    val_loader = DataLoader(val_set, batch_size=args.batch_size, shuffle=False, num_workers=0)

    print(f"Train: {train_n}, Val: {val_n}")

    model = NyxNNUEFast().to(device)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=5, factor=0.5)
    criterion = nn.MSELoss()

    best_val_loss = float("inf")
    patience_counter = 0
    max_patience = 15

    for epoch in range(args.epochs):
        # Training
        model.train()
        train_loss = 0.0
        train_batches = 0
        for features, targets, stm in train_loader:
            features = features.to(device)
            targets = targets.to(device)
            stm = stm.to(device)

            optimizer.zero_grad()
            pred = model(features, stm)
            loss = criterion(pred, targets)
            loss.backward()
            optimizer.step()

            train_loss += loss.item()
            train_batches += 1

        # Validation
        model.eval()
        val_loss = 0.0
        val_batches = 0
        with torch.no_grad():
            for features, targets, stm in val_loader:
                features = features.to(device)
                targets = targets.to(device)
                stm = stm.to(device)

                pred = model(features, stm)
                loss = criterion(pred, targets)
                val_loss += loss.item()
                val_batches += 1

        avg_train = train_loss / max(train_batches, 1)
        avg_val = val_loss / max(val_batches, 1)
        scheduler.step(avg_val)

        print(f"Epoch {epoch + 1}/{args.epochs}  Train: {avg_train:.6f}  Val: {avg_val:.6f}  LR: {optimizer.param_groups[0]['lr']:.6f}")

        if avg_val < best_val_loss:
            best_val_loss = avg_val
            patience_counter = 0
            quantize_and_export(model, args.output)
            print(f"  → Saved best model (val_loss={best_val_loss:.6f})")
        else:
            patience_counter += 1
            if patience_counter >= max_patience:
                print(f"Early stopping after {max_patience} epochs without improvement")
                break

    print(f"\nTraining complete. Best val loss: {best_val_loss:.6f}")
    print(f"NNUE file: {args.output}")


if __name__ == "__main__":
    main()
