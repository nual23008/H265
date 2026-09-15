#pragma once
#include <lib.h>
// TODO: khai báo hàm tính/tái tạo residual (prediction error)
int16_t* Residual(Block* original_block, Block* prediciton_block, int block_size);
