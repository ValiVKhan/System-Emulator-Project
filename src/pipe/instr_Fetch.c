/**************************************************************************
 * C S 429 system emulator
 * 
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
 **************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"

extern machine_t guest;
extern mem_status_t dmem_status;



extern uint64_t F_PC;

/*
 * Select PC logic.
 * STUDENT TO-DO:
 * Write the next PC to *current_PC.
 */

static comb_logic_t 
select_PC(uint64_t pred_PC,                                     // The predicted PC
          opcode_t D_opcode, uint64_t val_a,                    // Possible correction from RET
          opcode_t M_opcode, bool M_cond_val, uint64_t seq_succ,// Possible correction from B.cond
          uint64_t *current_PC) {
    /* 
     * Students: Please leave this code
     * at the top of this function. 
     * You may modify below it. 
     */
    if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR)
    {
        *current_PC = 0; // PC can't be 0 normally.
        return;
    }
    if (D_opcode == OP_RET) {
        *current_PC = val_a;
        return;
    }
    // Modify starting here.
    if (!(M_opcode != OP_B_COND) && M_cond_val == false) {
        // Set current program counter to the next sequential address
        *current_PC = seq_succ;
        // Return from the function
        return;
    }

    *current_PC = pred_PC;
    return;
}

/*
 * Predict PC logic. Conditional branches are predicted taken.
 * STUDENT TO-DO:
 * Write the predicted next PC to *predicted_PC
 * and the next sequential pc to *seq_succ.
 */

static comb_logic_t 
predict_PC(uint64_t current_PC, uint32_t insnbits, opcode_t op, 
           uint64_t *predicted_PC, uint64_t *seq_succ) {
    /* 
     * Students: Please leave this code
     * at the top of this function. 
     * You may modify below it. 
     */
    if (!current_PC) {
        return; // We use this to generate a halt instruction.
    }
    // Modify starting here.
    if (op == OP_B) {
    // Calculate the target address using the low bits of the instruction
        int64_t minBit = bitfield_s64(insnbits, 0, 26) * 4;
        // Set the predicted PC to the target address
        *predicted_PC = current_PC + minBit;
    }
    else if (op == OP_BL){
        // Calculate the target address using the low bits of the instruction
        int64_t minBit = bitfield_s64(insnbits, 0, 26) * 4;
        // Set the predicted PC to the target address
        *predicted_PC = current_PC + minBit;
    }
    else if (op == OP_B_COND) {
        // Calculate the target address using the low bits of the instruction
        int64_t minBit = bitfield_s64(insnbits, 5, 19) * 4;
        // Set the predicted PC to the target address
        *predicted_PC = current_PC + minBit;
    }
    else {
        // For other opcodes, increment the current program counter by 4
        *predicted_PC = current_PC + 4;
    }

    // Calculate the next sequential address
    *seq_succ = current_PC + 4;


    return;
}

/*
 * Helper function to recognize the aliased instructions:
 * LSL, LSR, CMP, and TST. We do this only to simplify the 
 * implementations of the shift operations (rather than having
 * to implement UBFM in full).
 */

static
void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {
    return;
}

/*
 * Fetch stage logic.
 * STUDENT TO-DO:
 * Implement the fetch stage.
 * 
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * Additionally, update F_PC for the next
 * cycle's predicted PC.
 * 
 * You will also need the following helper functions:
 * select_pc, predict_pc, and imem.
 */

comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {
    bool imem_err = 0;
    uint64_t current_PC;
    select_PC(in->pred_PC, X_in->print_op, X_in->val_a, M_out->print_op, M_out->cond_holds, M_out->seq_succ_PC, &current_PC);
    /* 
     * Students: This case is for generating HLT instructions
     * to stop the pipeline. Only write your code in the **else** case. 
     */
    if (!current_PC) {
        out->insnbits = 0xD4400000U;
        out->op = OP_HLT;
        out->print_op = OP_HLT;
        imem_err = false;
    }
    else {
        
        imem(current_PC, &out->insnbits, &imem_err);
        // Set instruction status to STAT_INS if there's an error in instruction memory
        in->status = imem_err ? STAT_INS : in->status;

        // Determine the current opcode using the instruction table
        opcode_t cur_code = itable[bitfield_u32(out->insnbits, 21, 11)];
        out->op = cur_code;

        // Predict the next PC based on the current opcode and instruction bits
        predict_PC(current_PC, out->insnbits, cur_code, &F_PC, &out->seq_succ_PC);

        // Set the print opcode based on the current opcode and instruction bits
        out->print_op = (cur_code == OP_ANDS_RR && bitfield_u32(out->insnbits, 0, 5) == 0x1FU)
                        ? OP_TST_RR
                        : (cur_code == OP_SUBS_RR && bitfield_u32(out->insnbits, 0, 5) == 0x1FU)
                        ? OP_CMP_RR
                        : cur_code;

        // Check if the current opcode is OP_ERROR, indicating an invalid instruction
        if (cur_code == OP_ERROR) {
            // Set the status of the current instruction to STAT_INS (invalid instruction)
            out->status = STAT_INS;
            // Set the status of the input instruction to STAT_INS as well
            in->status = STAT_INS;
        }
        out->this_PC = current_PC;
        out->status = in->status;
    }
    // If the output instruction's operation code is OP_HLT, the CPU is halted and the
    // status of both the input and output instructions is set to STAT_HLT.
    if (out->op == OP_HLT) {
        in->status = STAT_HLT;
        out->status = STAT_HLT;
    }
    return;
}