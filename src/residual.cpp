#include "residual.h"
#include "lib.h"
// TODO: implement

int16_t* Residual(Block* original_block, Block* prediciton_block, int block_size) {
    int16_t* res = new int16_t[block_size * block_size];
    for (int i = 0; i < block_size * block_size; i++) {
        res[i] = static_cast<int16_t>(original_block->data[i].data) - static_cast<int16_t>(prediciton_block->data[i].data);
    }   

    return res;
}