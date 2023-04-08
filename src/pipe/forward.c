/**************************************************************************
 * C S 429 system emulator
 *
 * forward.c - The value forwarding logic in the pipelined processor.
 **************************************************************************/

#include <stdbool.h>
#include "forward.h"

/* STUDENT TO-DO:
 * Implement forwarding register values from
 * execute, memory, and writeback back to decode.
 */
comb_logic_t forward_reg(uint8_t D_src1, uint8_t D_src2, uint8_t X_dst, uint8_t M_dst, uint8_t W_dst,
                         uint64_t X_val_ex, uint64_t M_val_ex, uint64_t M_val_mem, uint64_t W_val_ex,
                         uint64_t W_val_mem, bool M_wval_sel, bool W_wval_sel, bool X_w_enable,
                         bool M_w_enable, bool W_w_enable,
                         uint64_t *val_a, uint64_t *val_b)
{

      if ((W_w_enable && W_dst == D_src2 && W_dst != 36))
    {
        printf("Forward by 6\n");
        if(!W_wval_sel){
            *val_b = W_val_ex;
        } else{
            *val_b = W_val_mem;
        }
    }
     if (( M_w_enable && M_dst == D_src2 && M_dst != 36))
    {
        printf("Forward by 4\n");
        if(!M_wval_sel){
            *val_b = M_val_ex;
        } else{
            *val_b = M_val_mem;
        }
        
    }
     if ((W_dst == D_src1 && W_w_enable && W_dst != 36))
    {
        printf("Forward by 5\n");
        if(!W_wval_sel){
            *val_a = W_val_ex;
        } else{
            *val_a = W_val_mem;
        }
    }

  
       
     if (M_w_enable &&  M_dst != 36)
    {
        if (M_dst == D_src1){

        if(!M_wval_sel){
            *val_a = M_val_ex;
        } else{
            *val_a = M_val_mem;
        }        *val_a = !M_wval_sel ? M_val_ex : M_val_mem;
        }
    }
    
    if (X_w_enable &&  X_dst != 36)
    {
        if (D_src2 == X_dst){
            printf("Forward by 2\n");

            *val_b = X_val_ex;
        }
    }

    if (X_w_enable &&  X_dst != 36){
        if (D_src1 == X_dst){
            printf("Forward by 1\n");
            *val_a = X_val_ex;
        }
    }
   
     

    return;
}
