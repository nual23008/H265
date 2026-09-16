#!/usr/bin/env bash
# tools/prepare_input.sh
# Cắt một đoạn ngắn của file video (mp4/mkv/...) thành file YUV 4:2:0 8-bit thô cho encoder.
# Encoder chỉ đọc được YUV thô, và giải nén cả video sẽ rất nặng (1 frame 1080p = 3,1 MB).
#
# Cách dùng (Git Bash, đứng ở đâu cũng được):
#   bash tools/prepare_input.sh <file video> [giây bắt đầu] [số frame] [file .yuv ra] [bộ lọc ffmpeg]
#
# Ví dụ:
#   bash tools/prepare_input.sh Input/vid_demo.mp4 70 24 Input/vid_demo.yuv
#   bash tools/prepare_input.sh Input/vid_demo.mp4 70 24 Input/vid_demo_crop.yuv "crop=1920:800:0:140"
#
# Script in ra sẵn lệnh encoder tương ứng (kèm --size đúng với file vừa tạo).

set -e
cd "$(dirname "$0")/.."

VIDEO=${1:-Input/vid_demo.mp4}
START=${2:-0}
FRAMES=${3:-24}
OUT=${4:-Input/clip.yuv}
FILTER=${5:-}

# Tìm ffmpeg/ffprobe: ưu tiên PATH, sau đó các vị trí winget hay cài trên Windows
find_tool() {
    local name=$1
    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
        return
    fi
    local candidate
    for candidate in "$LOCALAPPDATA/Microsoft/WinGet/Links/$name.exe" \
                     "$LOCALAPPDATA"/Microsoft/WinGet/Packages/Gyan.FFmpeg*/ffmpeg*/bin/"$name.exe"; do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return
        fi
    done
    return 1
}

FFMPEG=$(find_tool ffmpeg) || {
    echo "Khong tim thay ffmpeg. Cai bang:  winget install Gyan.FFmpeg  roi mo lai terminal." >&2
    exit 1
}
FFPROBE=$(find_tool ffprobe) || FFPROBE=""

if [ ! -f "$VIDEO" ]; then
    echo "Khong thay file video '$VIDEO'." >&2
    exit 1
fi

# Bộ lọc: luôn ép về yuv420p; nếu người dùng truyền thêm filter (vd crop) thì nối vào trước
VF="format=yuv420p"
if [ -n "$FILTER" ]; then
    VF="$FILTER,format=yuv420p"
fi

echo "Cat $FRAMES frame tu '$VIDEO', bat dau o giay $START ..."
"$FFMPEG" -hide_banner -loglevel error -ss "$START" -i "$VIDEO" -frames:v "$FRAMES" -vf "$VF" -f rawvideo -y "$OUT"

# Suy ra kích thước frame từ file vừa tạo: cỡ file / số frame = W*H*1.5
SIZE_BYTES=$(stat -c %s "$OUT" 2>/dev/null || wc -c < "$OUT")
if [ -n "$FFPROBE" ]; then
    DIMS=$("$FFPROBE" -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0:s=x "$VIDEO")
    if [ -n "$FILTER" ]; then
        # Có filter (vd crop=1920:800:0:140) -> lấy kích thước từ chính filter nếu đọc được
        CROP=$(echo "$FILTER" | grep -oE 'crop=[0-9]+:[0-9]+' | head -1 | cut -d= -f2 | tr ':' 'x')
        [ -n "$CROP" ] && DIMS=$CROP
    fi
else
    DIMS="?x?"
fi

echo "Da ghi $OUT ($SIZE_BYTES byte, $FRAMES frame, $DIMS)"
echo
echo "Chay encoder:"
echo "  ./out/encoder.exe 32 --input $OUT --size $DIMS --out Output/$(basename "${OUT%.yuv}")"
echo "Xem thu:"
echo "  ffplay -f rawvideo -pixel_format yuv420p -video_size $DIMS $OUT"
