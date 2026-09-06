// include/quantization.h
#pragma once
#include "common.h"

// Residual --> [Transform] --> Coefficient --> [Quantization] --> Level
// Level --> [Dequantization] --> Coefficient' --> [Inverse Transform] --> Residual'

// level = round(coeff/Qstep)

// Theo paper H.265, dùng QP (Quantization Parameter) để xác định Qstep (Quantization Step Size)
// QP tăng lên 6 thì Qstep tăng gấp đôi, nên Qstep(QP) = Qstep(QP%6) * 2^(QP/6)

struct QpParam {
  int qp  = 0;
  int per = 0; // chu kỳ (period), cho biết nhân đôi bao nhiêu lần (đã đi qua bao nhiêu chu kỳ 6) --> 2^(QP/6)
  int rem = 0; // phần dư (remainder), cho biết dùng giá trị gốc nào trong 6 giá trị (40/45/51/57/64/72) --> Qstep(QP%6)
  explicit QpParam(int q) : qp(q), per(q / 6), rem(q % 6) {}
};

// Quantization: hệ số biến đổi (coefficient ) -> mức (level).
Blk quantize(const Blk& coeff, const QpParam& qp, int log2TrSize = kLog2N);

// Dequantization: level -> hệ số xấp xỉ (coefficient').
Blk dequantize(const Blk& level, const QpParam& qp, int log2TrSize = kLog2N);

inline Blk quantize(const Blk& coeff, int qp, int log2TrSize = kLog2N) {
  return quantize(coeff, QpParam(qp), log2TrSize);
}
inline Blk dequantize(const Blk& level, int qp, int log2TrSize = kLog2N) {
  return dequantize(level, QpParam(qp), log2TrSize);
}

double qStep(int qp);
