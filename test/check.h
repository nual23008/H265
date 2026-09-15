// test/check.h
// Công cụ kiểm tra tối giản cho các test, không cần thư viện ngoài.
#pragma once

#include <cmath>
#include <iostream>

// Số CHECK thất bại trong chương trình test hiện tại
inline int& failureCount() {
    static int count = 0;
    return count;
}

// CHECK(điều kiện): nếu sai thì in file:dòng và biểu thức, đếm lỗi, test vẫn chạy tiếp
#define CHECK(cond)                                                                        \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            ++failureCount();                                                              \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond << std::endl; \
        }                                                                                  \
    } while (0)

// CHECK_NEAR(a, b, eps): |a - b| <= eps (dùng cho số thực)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

// Gọi ở cuối main: in kết quả, trả về mã thoát (0 = qua hết)
inline int testResult(const char* name) {
    if (failureCount() == 0) {
        std::cout << "[PASS] " << name << std::endl;
        return 0;
    }
    std::cout << "[FAIL] " << name << ": " << failureCount() << " CHECK that bai" << std::endl;
    return 1;
}
