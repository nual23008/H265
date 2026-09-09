#include "residual.h"
#include "lib.h"
// TODO: implement

Block* Residual(Block* original_block, Block* prediciton_block, int block_size) {
    Block* residual = CreateBlock(block_size);
    for (int i = 0; i < block_size * block_size; i++) {
        residual->data[i].data = original_block->data[i].data - prediciton_block->data[i].data;
    }

    return residual;
}