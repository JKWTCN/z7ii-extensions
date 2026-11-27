"""Fit per-NP3 ISP adjustments from matched camera JPEG and NX Studio renders."""
# SPDX-License-Identifier: MIT
import argparse
import ctypes as C
from contextlib import ExitStack
from itertools import product
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
RGB_LIMIT = 512
CPXS_LIMIT = 128
HEADER = struct.Struct("<4sI")
ENTRY = struct.Struct("<II27h24hH")


def fingerprint(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def pack(records):
    if len(records) > 99:
        raise ValueError("At most 99 NP3 records fit in one calibration file")
    body = bytearray(HEADER.pack(b"NPC1", len(records)))
    seen = set()
    for record in records:
        key = record["np3_bytes"], int(record["np3_fnv1a"], 16)
        if not 8 <= key[0] < 4096 or key in seen:
            raise ValueError(f"Invalid or duplicate NP3: {record['np3']}")
        seen.add(key)
        values = []
        for name, rows, columns, limit in (("rgb", 3, 9, RGB_LIMIT), ("cpxs", 6, 4, CPXS_LIMIT)):
            table = record[name]
            if len(table) != rows or any(len(row) != columns for row in table):
                raise ValueError(f"Invalid {name} dimensions for {record['np3']}")
            for row in table:
                if any(type(value) is not int or abs(value) > limit for value in row):
                    raise ValueError(f"Invalid {name} coefficient for {record['np3']}")
                values.extend(row)
        body.extend(ENTRY.pack(*key, *values, 0))
    return body + struct.pack("<I", fingerprint(body))


MODEL_WRAPPER = r"""
#include "flexible_cloud.h"
#include "np3_calibration.h"
__declspec(dllexport) int generic(const unsigned char *np3,unsigned bytes,
                                  struct z72_style_fixed *style) {
    struct z72_np3_flexible profile;
    if (z72_np3_parse(np3,bytes,&profile,0x080fu) ||
        z72_cloud_prepare(&profile,style)) return -1;
    return z72_cloud_chroma_mode(&profile);
}
__declspec(dllexport) void correct(const struct z72_style_fixed *base,
                                   const struct z72_np3_calibration *record,
                                   struct z72_style_fixed *out) {
    *out=*base;
    z72_cal_apply(out,record);
}
__declspec(dllexport) unsigned verify(const unsigned char *data,unsigned bytes) {
    union z72_np3_calibration_block block;
    if(bytes>=sizeof(block.data)) return 0;
    z72_style_copy(block.data,data,bytes);
    return z72_cal_count(&block,bytes);
}
"""


def load_model(fixtures, folder, cleanup):
    from flexible_nx_reference import NX
    from flexible_style_fit import BODY_SHA, ROME_SHA, WRAPPER, Evaluator, digest

    dll = NX / "Rome2.dll"
    body = (fixtures / "body.bin").read_bytes()
    if digest(dll.read_bytes()) != ROME_SHA or digest(body) != BODY_SHA:
        raise ValueError("Expected the original Z 7II model fixtures and matching NX Studio")
    gcc = shutil.which("gcc")
    if not gcc:
        raise ValueError("The model requires 64-bit GCC")
    cleanup.enter_context(os.add_dll_directory(str(NX)))
    cleanup.enter_context(os.add_dll_directory(str(Path(gcc).parent)))
    source, binary = folder / "colour.c", folder / "colour.dll"
    source.write_text(WRAPPER + MODEL_WRAPPER, encoding="ascii")
    subprocess.run([gcc, "-O2", "-shared", "-static-libgcc", "-I", str(ROOT / "sdk"),
                    str(source), "-o", str(binary)], check=True)
    library = C.CDLL(str(binary))
    cleanup.callback(C.windll.kernel32.FreeLibrary, C.c_void_p(library._handle))
    library.expand.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    library.felsite.argtypes = [C.c_void_p, C.c_void_p, C.c_uint, C.c_void_p, C.c_uint, C.c_void_p, C.c_void_p]
    library.generic.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]
    library.correct.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    library.verify.argtypes = [C.c_void_p, C.c_uint]
    library.verify.restype = C.c_uint
    rome = C.WinDLL(str(dll))
    cleanup.callback(C.windll.kernel32.FreeLibrary, C.c_void_p(rome._handle))
    return Evaluator(library, rome._handle + 0x2B8960, rome._handle + 0x2B8250,
                     rome._handle + 0x2AE600, body, (fixtures / "neutral-body.bin").read_bytes(),
                     (fixtures / "param.bin").read_bytes())


def image_samples(camera, reference):
    import numpy as np
    from PIL import Image
    from scipy.ndimage import maximum_filter, minimum_filter
    from flexible_style_fit import lab

    with Image.open(reference) as ref, Image.open(camera) as jpg:
        if jpg.width * ref.height != jpg.height * ref.width:
            raise ValueError(f"{camera.name}: reference framing differs")
        wanted = np.asarray(ref.convert("RGB"))
        observed = np.asarray(jpg.convert("RGB").resize(ref.size, Image.Resampling.LANCZOS))
    y, x = np.mgrid[2:observed.shape[0]-2:6, 2:observed.shape[1]-2:6]
    # Exclude pixels where sharpening or local edges dominate the colour difference.
    flat = np.ones(x.shape, dtype=bool)
    for rgb in (observed, wanted):
        for channel in range(3):
            plane = rgb[:, :, channel]
            span = maximum_filter(plane, 3) - minimum_filter(plane, 3)
            flat &= span[y, x] <= 14
    heldout = ((x // 40 + 3 * (y // 40)) % 4 == 0)[flat]
    return lab(observed[y[flat], x[flat]] / 255.), lab(wanted[y[flat], x[flat]] / 255.), heldout


def fit_case(model, data, camera, reference):
    import numpy as np
    from scipy.optimize import lsq_linear
    from scipy.spatial import cKDTree
    from flexible_style_fit import metrics

    style_size = (3 * 129 + 6 * 32) * 2
    base = C.create_string_buffer(style_size)
    corrected = C.create_string_buffer(style_size)
    mode = model.library.generic(data, len(data), base)
    if mode < 0:
        raise ValueError(f"{camera.name}: unsupported Flexible Color recipe")
    monochrome = mode != 0
    count = 9 if monochrome else 51
    limits = np.r_[np.full(min(count, 27), RGB_LIMIT), np.full(max(0, count - 27), CPXS_LIMIT)]
    paired = model.base_pairs.astype(int)
    low, high = -512 - paired.min(axis=1), 511 - paired.max(axis=1)

    def coefficients(vector):
        fitted = np.rint(vector).astype(int)
        if monochrome:
            return np.tile(fitted, 3).reshape(3, 9), np.zeros((6, 4), dtype=int)
        return fitted[:27].reshape(3, 9), fitted[27:].reshape(6, 4)

    def evaluate(vector, edge, points, nodes):
        rgb, cpxs = coefficients(vector)
        record = ENTRY.pack(0, 0, *rgb.ravel(), *cpxs.ravel(), 0)
        model.library.correct(base, record, corrected)
        tables = np.frombuffer(corrected, dtype="<i2")
        model.set_tables(tables[:387].reshape(3, 129),
                         np.clip(tables[387:].reshape(6, 32), low, high), nodes)
        if monochrome:
            # The camera clears the first four paired rows before upload.
            C.memset(C.addressof(model.body) + 24732, 0, 4 * 2 * 32 * 2)
            C.memset(C.addressof(model.body) + 395584, 0, 4 * 32 * 2)
        return model.evaluate(edge, points)

    observed, target, heldout = image_samples(camera, reference)
    zero = np.zeros(count)
    edge = 41
    gamut = np.asarray(list(product(range(edge), repeat=3)), dtype="<i4")
    baseline = evaluate(zero, edge, gamut, 129)
    distance, neighbors = cKDTree(baseline).query(observed, k=4)
    nearest, indices = np.unique(neighbors, return_inverse=True)
    indices = indices.reshape(neighbors.shape)
    points = gamut[nearest]
    weight = (distance + .75) ** -2
    weight /= weight.sum(axis=1, keepdims=True)
    original = {129: baseline[nearest], 65: evaluate(zero, edge, points, 65)}

    def estimate(vector, nodes):
        difference = evaluate(vector, edge, points, nodes) - original[nodes]
        return observed + (difference[indices] * weight[..., None]).sum(axis=1)

    train = ~heldout & (distance[:, 0] <= 6)
    if train.sum() < 100 or heldout.sum() < 100:
        raise ValueError(f"{camera.name}: too few matched samples")
    fit_channels = slice(0, 1) if monochrome else slice(0, 3)
    target_train = target[train, fit_channels]
    # Penalize steep changes between neighboring knots and large magnitudes.
    penalties = np.diag(1. / limits)
    rgb_count = 9 if monochrome else 27
    smooth = np.diff(np.eye(count)[:rgb_count].reshape(-1, 9, count), n=2, axis=1)
    penalties = np.vstack((penalties, smooth.reshape(-1, count) / 130.))
    baseline_score = metrics(observed[heldout], target[heldout])
    best = zero
    best_photo, best_preview = baseline_score, baseline_score
    current = zero
    for _ in range(3):
        fitted = estimate(current, 129)[train, fit_channels]
        columns = []
        for step in np.eye(count) * 24:
            shifted = np.clip(current + step, -limits, limits)
            if np.array_equal(shifted, current):
                shifted = np.clip(current - step, -limits, limits)
            size = (shifted - current).sum()
            columns.append(((estimate(shifted, 129)[train, fit_channels] - fitted) / size).ravel())
        scale = np.sqrt(len(fitted))
        matrix = np.vstack((np.column_stack(columns) / scale, penalties))
        residual = np.r_[(target_train - fitted).ravel() / scale, -penalties @ current]
        change = lsq_linear(matrix, residual, bounds=(-limits - current, limits - current)).x
        for strength in (.35, .65, 1.):
            candidate = np.rint(current + strength * change)
            photo = metrics(estimate(candidate, 129)[heldout], target[heldout])
            preview = metrics(estimate(candidate, 65)[heldout], target[heldout])
            if (photo["mean_deltaE76"] < best_photo["mean_deltaE76"] and
                    preview["mean_deltaE76"] < baseline_score["mean_deltaE76"] and
                    max(photo["p95_deltaE76"], preview["p95_deltaE76"]) <= baseline_score["p95_deltaE76"]):
                best, best_photo, best_preview = candidate, photo, preview
        if np.array_equal(best, current):
            break
        current = best
    rgb, cpxs = coefficients(best)
    return dict(rgb=rgb.tolist(), cpxs=cpxs.tolist(), training_samples=int(train.sum()),
                heldout_camera=baseline_score, heldout_estimated_photo=best_photo,
                heldout_estimated_preview=best_preview)


def matched_profiles(folder, frames):
    from np3_compatibility import records

    profiles = []
    for path in sorted((folder / "CUSTOMPC").iterdir()):
        if path.suffix.lower() == ".np3":
            tags = {tag: value for tag, _, value in records(path.read_bytes())}
            profiles.append((path, tags[0x200].rstrip(b"\0"), tags[0x300]))
    for frame in frames:
        jpeg = folder / (frame + ".JPG")
        with jpeg.open("rb") as stream:
            metadata = stream.read(16000)
        matches = [(path, profile_base) for path, name, profile_base in profiles
                   if b"03000300" + name.ljust(20, b"\0") in metadata]
        if len(matches) != 1:
            raise ValueError(f"{frame}: expected exactly one NP3 name in the JPEG metadata")
        path, profile_base = matches[0]
        yield frame, jpeg, path, profile_base


def fit(args):
    research = args.research.resolve()
    sys.path.insert(0, str(research / "tools"))
    records = []
    skipped = []
    with ExitStack() as cleanup:
        work = Path(cleanup.enter_context(TemporaryDirectory(prefix="z72-np3-fit-")))
        model = load_model(args.fixtures, work, cleanup)
        for frame, jpeg, profile, profile_base in matched_profiles(args.samples, args.frames):
            if profile_base != b"\x00\x20":
                skipped.append(dict(frame=frame, np3=profile.name, base=profile_base.hex()))
                continue
            if args.references:
                reference = args.references / (frame + ".ppm")
            else:
                reference = work / (frame + ".ppm")
                subprocess.run([sys.executable, "-B", str(research / "tools" / "flexible_nx_reference.py"),
                                str(args.samples / (frame + ".NEF")), "--full-profile", str(profile),
                                "--width", "720", "--output", str(reference)], check=True, stdout=subprocess.DEVNULL)
            data = profile.read_bytes()
            record = dict(frame=frame, np3=profile.name, np3_bytes=len(data), np3_fnv1a=f"{fingerprint(data):08x}")
            record.update(fit_case(model, data, jpeg, reference))
            records.append(record)
            print(f"{frame} {profile.name}: {record['heldout_camera']['mean_deltaE76']:.2f} -> "
                  f"{record['heldout_estimated_photo']['mean_deltaE76']:.2f}", flush=True)
        if not records:
            raise ValueError("No Flexible Color NP3s matched the supplied exposures")
        packed = pack(records)
        if model.library.verify(bytes(packed), len(packed)) != len(records):
            raise ValueError("The camera calibration reader rejected the fitted data")
    with args.report.open("x", encoding="utf-8") as report:
        json.dump(dict(records=records, native_profiles=skipped,
                       heldout="flat spatial blocks from the matched exposure, model-differential estimate"),
                  report, ensure_ascii=False, indent=2)
        report.write("\n")
    with args.output.open("xb") as output:
        output.write(packed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    fit_cmd = commands.add_parser("fit")
    for flag in ("research", "fixtures", "samples", "report", "output"):
        fit_cmd.add_argument("--" + flag, type=Path, required=True)
    fit_cmd.add_argument("--references", type=Path, help="Use already rendered NX Studio PPMs")
    fit_cmd.add_argument("--frames", nargs="+", required=True, help="JPEG/NEF filename stems")
    pack_cmd = commands.add_parser("pack")
    pack_cmd.add_argument("report", type=Path)
    pack_cmd.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.command == "fit":
        if args.report.exists() or args.output.exists():
            parser.error("Choose new output paths")
        fit(args)
    else:
        records = json.loads(args.report.read_text(encoding="utf-8"))["records"]
        packed = pack(records)
        with args.output.open("xb") as output:
            output.write(packed)


if __name__ == "__main__":
    main()
