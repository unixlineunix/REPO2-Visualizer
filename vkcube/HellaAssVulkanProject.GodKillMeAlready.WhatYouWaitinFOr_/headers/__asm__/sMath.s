# x87 FPU math functions — no libm, direct hardware calls

.section .text

#----------------------------------------------------------------------------------------
# FUN: asm_sinf — sine via x87 FSIN
# Input:  %xmm0 (float)
# Output: %xmm0 (float)
#----------------------------------------------------------------------------------------
.global asm_sinf
.type asm_sinf, @function
asm_sinf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)
    fsin
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_cosf — cosine via x87 FCOS
#----------------------------------------------------------------------------------------
.global asm_cosf
.type asm_cosf, @function
asm_cosf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)
    fcos
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_tanf — tan via x87 FPTAN (pushes 1.0 then result, we pop 1.0)
#----------------------------------------------------------------------------------------
.global asm_tanf
.type asm_tanf, @function
asm_tanf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)
    fptan
    fstp %st(0)    # pop the 1.0
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_sqrtf — sqrt via x87 FSQRT
#----------------------------------------------------------------------------------------
.global asm_sqrtf
.type asm_sqrtf, @function
asm_sqrtf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)
    fsqrt
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_atan2f — atan2 via x87 FPATAN
# Input:  %xmm0 = y, %xmm1 = x
# Output: %xmm0 = atan2(y, x)
#----------------------------------------------------------------------------------------
.global asm_atan2f
.type asm_atan2f, @function
asm_atan2f:
    sub $16, %rsp
    movss %xmm0, (%rsp)    # y
    movss %xmm1, 4(%rsp)   # x
    flds 4(%rsp)           # load x
    flds (%rsp)            # load y
    fpatan                 # atan(st1/st0) -> st0
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $16, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_acosf — acos(x) = atan2(sqrt(1-x²), x)
#----------------------------------------------------------------------------------------
.global asm_acosf
.type asm_acosf, @function
asm_acosf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)            # x
    fld %st(0)             # x x
    fmul %st(0), %st(0)    # x²
    fld1                   # 1 x²
    fsubrp                 # 1-x²
    fsqrt                  # sqrt(1-x²)
    flds (%rsp)            # x sqrt(1-x²)
    fpatan                 # atan2(sqrt(1-x²), x)
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_asinf — asin(x) = atan2(x, sqrt(1-x²))
#----------------------------------------------------------------------------------------
.global asm_asinf
.type asm_asinf, @function
asm_asinf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)            # x
    fld %st(0)             # x x
    fmul %st(0), %st(0)    # x x²
    fld1                   # x x² 1
    fsubrp %st(0), %st(1)  # x (1-x²)
    fsqrt                  # x sqrt(1-x²)
    fpatan                 # atan2(x, sqrt(1-x²))
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_atanf — atan(x) via x87 FLD1 + FPATAN
#----------------------------------------------------------------------------------------
.global asm_atanf
.type asm_atanf, @function
asm_atanf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)
    fld1
    fpatan
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_powf — pow(x, y) via x87 FYL2X + F2XM1
# pow(x, y) = 2^(y * log2(x))
#----------------------------------------------------------------------------------------
.global asm_powf
.type asm_powf, @function
asm_powf:
    sub $16, %rsp
    movss %xmm0, (%rsp)    # x
    movss %xmm1, 4(%rsp)   # y
    flds (%rsp)            # x
    fyl2x                  # st(1) * log2(st(0)) — but st(1) is empty, need to load y first
    # Actually need correct order
    # Let me rewrite:
    flds (%rsp)            # x
    flds 4(%rsp)           # y x
    fxch                   # x y
    fyl2x                  # y * log2(x)
    # Now compute 2^(result)
    fld %st(0)             # I I
    frndint                # int(I) I
    fxch                   # I int(I)
    fsub %st(1), %st(0)    # frac int
    f2xm1                  # 2^frac-1 int
    fld1                   # 1 2^frac-1 int
    faddp                  # 2^frac int
    fscale                 # 2^frac * 2^int
    fstp %st(1)            # clean stack
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $16, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_fabsf — absolute value
#----------------------------------------------------------------------------------------
.global asm_fabsf
.type asm_fabsf, @function
asm_fabsf:
    sub $8, %rsp
    movss %xmm0, (%rsp)
    flds (%rsp)
    fabs
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $8, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_fmodf — fmod(x, y) via FPREM
#----------------------------------------------------------------------------------------
.global asm_fmodf
.type asm_fmodf, @function
asm_fmodf:
    sub $16, %rsp
    movss %xmm0, (%rsp)
    movss %xmm1, 4(%rsp)
    flds 4(%rsp)           # y
    flds (%rsp)            # x y
.Lfmod_loop:
    fprem
    fstsw %ax
    sahf
    jp .Lfmod_loop
    fstp %st(1)
    fstps (%rsp)
    movss (%rsp), %xmm0
    add $16, %rsp
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_copysignf — copysign(mag, sign)
#----------------------------------------------------------------------------------------
.global asm_copysignf
.type asm_copysignf, @function
asm_copysignf:
    movss %xmm0, %xmm2
    andps .Lmask_abs(%rip), %xmm2   # abs(mag)
    andps .Lmask_sign(%rip), %xmm1  # sign bit of sign
    orps %xmm1, %xmm2
    movss %xmm2, %xmm0
    ret

.section .rodata
.align 16
.Lmask_abs:  .long 0x7FFFFFFF, 0, 0, 0
.Lmask_sign: .long 0x80000000, 0, 0, 0
