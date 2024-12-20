#include "common.h"

// Define macros to output the right encoding, as the assembler doesn't
// understand mnemonics yet. An issue with this is that we need to write
// everything with a massive inline asm, as we need to pass the exact register
// numbers in. That means we need control over the registers right from the
// start
#define STRINGIFY(s) STRINGIFY_BASE(s)
#define STRINGIFY_BASE(s) #s
#define VZIPEVEN_ENCODING(vd, vs2, vs1)                                        \
  (0b001100 << 26) | (1 << 25) | (vs2 << 20) | (vs1 << 15) | (0b000 << 12) |   \
      (vd << 7) | 0b1011011
#define VZIPEVEN(vd, vs2, vs1)                                                 \
  ".word " STRINGIFY(VZIPEVEN_ENCODING(vd, vs2, vs1))
#define VZIPODD_ENCODING(vd, vs2, vs1)                                         \
  (0b011100 << 26) | (1 << 25) | (vs2 << 20) | (vs1 << 15) | (0b000 << 12) |   \
      (vd << 7) | 0b1011011
#define VZIPODD(vd, vs2, vs1) ".word " STRINGIFY(VZIPODD_ENCODING(vd, vs2, vs1))

#define VZIP2A_ENCODING(vd, vs2, vs1)                                          \
  (0b000100 << 26) | (1 << 25) | (vs2 << 20) | (vs1 << 15) | (0b000 << 12) |   \
      (vd << 7) | 0b1011011
#define VZIP2A(vd, vs2, vs1) ".word " STRINGIFY(VZIP2A_ENCODING(vd, vs2, vs1))
#define VZIP2B_ENCODING(vd, vs2, vs1)                                          \
  (0b010100 << 26) | (1 << 25) | (vs2 << 20) | (vs1 << 15) | (0b000 << 12) |   \
      (vd << 7) | 0b1011011
#define VZIP2B(vd, vs2, vs1) ".word " STRINGIFY(VZIP2B_ENCODING(vd, vs2, vs1))

// clang-format off
// Transpose 4 lots of 2x16 blocks.
static inline __attribute__((always_inline))
void transpose_2x16x4(IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Conceptually, this is the same as an 8x16 transpose, which (for now)
  // we do as two 8x8 transposes
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a0, %[a_in]\n"     \
      "  mv a1, %[lda_in]\n"     \
      "  mv a2, %[b_in]\n"       \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  # We do this transpose as two blocks of 8x8\n"   \
      "  # lda in bytes\n"   \
      "  slli t0, a1, 2\n"   \
      "  # 2 * lda\n"   \
      "  slli t1, a1, 3\n"   \
      "  # stride_out * 1 = 16 * 4 bytes = 64\n"   \
      "  li t2, 64\n"   \
      "  # stride_out * 2 = 16 * 8 bytes = 128\n"   \
      "  li t3, 128\n"   \
      "  add a3, a0, t0\n"   \
      "  add a4, a0, t1\n"   \
      "  add a5, a3, t1\n"   \
      "  vle32.v v1, (a0)\n"   \
      "  vle32.v v2, (a3)\n"   \
      "  vle32.v v3, (a4)\n"   \
      "  vle32.v v4, (a5)\n"   \
      "  \n"   \
      "  add a0, a4, t1\n"   \
      "  add a3, a0, t0\n"   \
      "  add a4, a0, t1\n"   \
      "  add a5, a3, t1\n"   \
      "  vle32.v v5, (a0)\n"   \
      "  vle32.v v6, (a3)\n"   \
      "  vle32.v v7, (a4)\n"   \
      "  vle32.v v8, (a5)\n"   \
      "  # also get ready for the next block of 8 columns to load\n"   \
      "  add a0, a5, t0\n"   \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  # v3 = [a20, a21, a22, a23, a24, a25, a26, a27]\n"   \
      "  # v4 = [a30, a31, a32, a33, a34, a35, a36, a37]\n"   \
      "  # v5 = [a40, a41, a42, a43, a44, a45, a46, a47]\n"   \
      "  # v6 = [a50, a51, a52, a53, a54, a55, a56, a57]\n"   \
      "  # v7 = [a60, a61, a62, a63, a64, a65, a66, a67]\n"   \
      "  # v8 = [a70, a71, a72, a73, a74, a75, a76, a77]\n"   \
      VZIPEVEN(9, 1, 2) "\n"   \
      VZIPODD(10, 1, 2) "\n"   \
      VZIPEVEN(11, 3, 4) "\n"   \
      VZIPODD(12, 3, 4) "\n"   \
      VZIPEVEN(13, 5, 6) "\n"   \
      VZIPODD(14, 5, 6) "\n"   \
      VZIPEVEN(15, 7, 8) "\n"   \
      VZIPODD(16, 7, 8) "\n"   \
      "  # v9 =  [a00, a10, a02, a12, a04, a14, a06, a16]\n"   \
      "  # v10 = [a01, a11, a03, a13, a05, a15, a07, a17]\n"   \
      "  # v11 = [a20, a30, a22, a32, a24, a34, a26, a36]\n"   \
      "  # v12 = [a21, a31, a23, a33, a25, a35, a27, a37]\n"   \
      "  # v13 = [a40, a50, a42, a52, a44, a54, a46, a56]\n"   \
      "  # v14 = [a41, a51, a43, a53, a45, a55, a47, a57]\n"   \
      "  # v15 = [a60, a70, a62, a72, a64, a74, a66, a76]\n"   \
      "  # v16 = [a61, a71, a63, a73, a65, a75, a67, a77]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIPEVEN(1, 9, 11) "\n"   \
      VZIPEVEN(2, 10, 12) "\n"   \
      VZIPODD(3, 9, 11) "\n"   \
      VZIPODD(4, 10, 12) "\n"   \
      VZIPEVEN(5, 13, 15) "\n"   \
      VZIPEVEN(6, 14, 16) "\n"   \
      VZIPODD(7, 13, 15) "\n"   \
      VZIPODD(8, 14, 16) "\n"   \
      "  # v1 = [a00, a10, a20, a30, a04, a14, a24, a34]\n"   \
      "  # v2 = [a01, a11, a21, a31, a05, a15, a25, a35]\n"   \
      "  # v3 = [a02, a12, a22, a32, a06, a16, a26, a36]\n"   \
      "  # v4 = [a03, a13, a23, a33, a07, a17, a27, a37]\n"   \
      "  # v5 = [a40, a50, a60, a70, a44, a54, a64, a74]\n"   \
      "  # v6 = [a41, a51, a61, a71, a45, a55, a65, a75]\n"   \
      "  # v7 = [a42, a52, a62, a72, a46, a56, a66, a76]\n"   \
      "  # v8 = [a43, a53, a63, a73, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Now deal with 2 * 64 = 128 bits at a time\n"   \
      "  li a5, 3\n"   \
      "  vmv.s.x v0, a5\n"   \
      "  \n"   \
      "  vslidedown.vi v9, v1, 2\n"   \
      "  vslidedown.vi v10, v2, 2\n"   \
      "  vslidedown.vi v11, v3, 2\n"   \
      "  vslidedown.vi v12, v4, 2\n"   \
      "  vslideup.vi v1, v5, 2\n"   \
      "  vslideup.vi v2, v6, 2\n"   \
      "  vslideup.vi v3, v7, 2\n"   \
      "  vslideup.vi v4, v8, 2\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v9 = [a04, a14, a24, a34,   ?,   ?,   ?,   ?]\n"   \
      "  # v10 = [a05, a15, a25, a35,   ?,   ?,   ?,   ?]\n"   \
      "  # v11 = [a06, a16, a26, a36,   ?,   ?,   ?,   ?]\n"   \
      "  # v12 = [a07, a17, a27, a37,   ?,   ?,   ?,   ?]\n"   \
      "  \n"   \
      "  vmerge.vvm v5, v5, v9, v0\n"   \
      "  vmerge.vvm v6, v6, v10, v0\n"   \
      "  vmerge.vvm v7, v7, v11, v0\n"   \
      "  vmerge.vvm v8, v8, v12, v0\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v5 = [a04, a14, a24, a34, a44, a54, a64, a74]\n"   \
      "  # v6 = [a05, a15, a25, a35, a45, a55, a65, a75]\n"   \
      "  # v7 = [a06, a16, a26, a36, a46, a56, a66, a76]\n"   \
      "  # v8 = [a07, a17, a27, a37, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a3, a2, t2\n"   \
      "  add a4, a2, t3\n"   \
      "  add a5, a3, t3\n"   \
      "  vse64.v v1, (a2)\n"   \
      "  vse64.v v2, (a3)\n"   \
      "  vse64.v v3, (a4)\n"   \
      "  vse64.v v4, (a5)\n"   \
      "  \n"   \
      "  add a3, a4, t3\n"   \
      "  add a4, a3, t2\n"   \
      "  add a5, a3, t3\n"   \
      "  add a6, a4, t3\n"   \
      "  vse64.v v5, (a3)\n"   \
      "  vse64.v v6, (a4)\n"   \
      "  vse64.v v7, (a5)\n"   \
      "  vse64.v v8, (a6)\n"   \
      "  \n"   \
      "  # And now the next block of 8x8\n"   \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  # Move the output pointer one vector register of bytes ahead\n"   \
      "  addi a2, a2, 32\n"   \
      "  \n"   \
      "  add a3, a0, t0\n"   \
      "  add a4, a0, t1\n"   \
      "  add a5, a3, t1\n"   \
      "  vle32.v v1, (a0)\n"   \
      "  vle32.v v2, (a3)\n"   \
      "  vle32.v v3, (a4)\n"   \
      "  vle32.v v4, (a5)\n"   \
      "  \n"   \
      "  add a3, a4, t1\n"   \
      "  add a4, a3, t0\n"   \
      "  add a5, a3, t1\n"   \
      "  add a6, a4, t1\n"   \
      "  vle32.v v5, (a3)\n"   \
      "  vle32.v v6, (a4)\n"   \
      "  vle32.v v7, (a5)\n"   \
      "  vle32.v v8, (a6)\n"   \
      "  # v1 =  [a08, a09, a0a, a0b, a0c, a0d, a0e, a0f]\n"   \
      "  # v2 =  [a18, a19, a1a, a1b, a1c, a1d, a1e, a1f]\n"   \
      "  # v3 =  [a28, a29, a2a, a2b, a2c, a2d, a2e, a2f]\n"   \
      "  # v4 =  [a38, a39, a3a, a3b, a3c, a3d, a3e, a3f]\n"   \
      "  # v5 =  [a48, a49, a4a, a4b, a4c, a4d, a4e, a4f]\n"   \
      "  # v6 =  [a58, a59, a5a, a5b, a5c, a5d, a5e, a5f]\n"   \
      "  # v7 =  [a68, a69, a6a, a6b, a6c, a6d, a6e, a6f]\n"   \
      "  # v8 =  [a78, a79, a7a, a7b, a7c, a7d, a7e, a7f]\n"   \
      VZIPEVEN(9, 1, 2) "\n"   \
      VZIPODD(10, 1, 2) "\n"   \
      VZIPEVEN(11, 3, 4) "\n"   \
      VZIPODD(12, 3, 4) "\n"   \
      VZIPEVEN(13, 5, 6) "\n"   \
      VZIPODD(14, 5, 6) "\n"   \
      VZIPEVEN(15, 7, 8) "\n"   \
      VZIPODD(16, 7, 8) "\n"   \
      "  # v9 =  [a08, a18, a0a, a1a, a0c, a1c, a0e, a1e]\n"   \
      "  # v10 = [a09, a19, a0b, a1b, a0d, a1d, a0f, a1f]\n"   \
      "  # v11 = [a28, a38, a2a, a3a, a2c, a3c, a2e, a3e]\n"   \
      "  # v12 = [a29, a39, a2b, a3b, a2d, a3d, a2f, a3f]\n"   \
      "  # v13 = [a48, a58, a4a, a5a, a4c, a5c, a4e, a5e]\n"   \
      "  # v14 = [a49, a59, a4b, a5b, a4d, a5d, a4f, a5f]\n"   \
      "  # v15 = [a68, a78, a6a, a7a, a6c, a7c, a6e, a7e]\n"   \
      "  # v16 = [a69, a79, a6b, a7b, a6d, a7d, a6f, a7f]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIPEVEN(1, 9, 11) "\n"   \
      VZIPEVEN(2, 10, 12) "\n"   \
      VZIPODD(3, 9, 11) "\n"   \
      VZIPODD(4, 10, 12) "\n"   \
      VZIPEVEN(5, 13, 15) "\n"   \
      VZIPEVEN(6, 14, 16) "\n"   \
      VZIPODD(7, 13, 15) "\n"   \
      VZIPODD(8, 14, 16) "\n"   \
      "  # v1 = [a08, a18, a2a, a38, a0c, a1c, a2c, a3c]\n"   \
      "  # v2 = [a09, a19, a29, a39, a0d, a1d, a2d, a3d]\n"   \
      "  # v3 = [a0a, a1a, a2a, a3a, a0e, a1e, a2e, a3e]\n"   \
      "  # v4 = [a0b, a1b, a2b, a3b, a0f, a1f, a2f, a3f]\n"   \
      "  # v5 = [a48, a58, a68, a78, a4c, a5c, a6c, a7c]\n"   \
      "  # v6 = [a49, a59, a69, a79, a4d, a5d, a6d, a7d]\n"   \
      "  # v7 = [a4a, a5a, a6a, a7a, a4e, a5e, a6e, a7e]\n"   \
      "  # v8 = [a4b, a5b, a6b, a7b, a4f, a5f, a6f, a7f]\n"   \
      "  \n"   \
      "  vslidedown.vi v9, v1, 2\n"   \
      "  vslidedown.vi v10, v2, 2\n"   \
      "  vslidedown.vi v11, v3, 2\n"   \
      "  vslidedown.vi v12, v4, 2\n"   \
      "  vslideup.vi v1, v5, 2\n"   \
      "  vslideup.vi v2, v6, 2\n"   \
      "  vslideup.vi v3, v7, 2\n"   \
      "  vslideup.vi v4, v8, 2\n"   \
      "  # v1 = [a08, a18, a28, a38, a48, a58, a68, a78]\n"   \
      "  # v2 = [a09, a19, a29, a39, a49, a59, a69, a79]\n"   \
      "  # v3 = [a0a, a1a, a2a, a3a, a4a, a5a, a6a, a7a]\n"   \
      "  # v4 = [a0b, a1b, a2b, a3b, a4b, a5b, a6b, a7b]\n"   \
      "  # v9 = [a0c, a1c, a2c, a3c,   ?,   ?,   ?,   ?]\n"   \
      "  # v10 = [a0d, a1d, a2d, a3d,   ?,   ?,   ?,   ?]\n"   \
      "  # v11 = [a0e, a1e, a2e, a3e,   ?,   ?,   ?,   ?]\n"   \
      "  # v12 = [a0f, a1f, a2f, a3f,   ?,   ?,   ?,   ?]\n"   \
      "  \n"   \
      "  vmerge.vvm v5, v5, v9, v0\n"   \
      "  vmerge.vvm v6, v6, v10, v0\n"   \
      "  vmerge.vvm v7, v7, v11, v0\n"   \
      "  vmerge.vvm v8, v8, v12, v0\n"   \
      "  # v1 = [a08, a18, a28, a38, a48, a58, a68, a78]\n"   \
      "  # v2 = [a09, a19, a29, a39, a49, a59, a69, a79]\n"   \
      "  # v3 = [a0a, a1a, a2a, a3a, a4a, a5a, a6a, a7a]\n"   \
      "  # v4 = [a0b, a1b, a2b, a3b, a4b, a5b, a6b, a7b]\n"   \
      "  # v5 = [a0c, a1c, a2c, a3c, a4c, a5c, a6c, a7c]\n"   \
      "  # v6 = [a0d, a1d, a2d, a3d, a4d, a5d, a6d, a7d]\n"   \
      "  # v7 = [a0e, a1e, a2e, a3e, a4e, a5e, a6e, a7e]\n"   \
      "  # v8 = [a0f, a1f, a2f, a3f, a4f, a5f, a6f, a7f]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a3, a2, t2\n"   \
      "  add a4, a2, t3\n"   \
      "  add a5, a3, t3\n"   \
      "  vse64.v v1, (a2)\n"   \
      "  vse64.v v2, (a3)\n"   \
      "  vse64.v v3, (a4)\n"   \
      "  vse64.v v4, (a5)\n"   \
      "  \n"   \
      "  add a3, a4, t3\n"   \
      "  add a4, a3, t2\n"   \
      "  add a5, a3, t3\n"   \
      "  add a6, a4, t3\n"   \
      "  vse64.v v5, (a3)\n"   \
      "  vse64.v v6, (a4)\n"   \
      "  vse64.v v7, (a5)\n"   \
      "  vse64.v v8, (a6)\n"   \
      // Output operands
      :
      // Input operands
      : [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a0", "a1", "a2", "a3", "a4", "a5", "a6",
        "t0", "t1", "t2", "t3",
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8",
        "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16"
  );
}

static inline __attribute__((always_inline))
void transpose_2x16xm(int m, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Conceptually, this is the same as a 2m x 16 transpose, which (for now)
  // we perform as two 2m x 8 transposes
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a1, %[m_in]\n"     \
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  # We set the vector length to the number of remaining rows\n"   \
      "  vsetvli zero, a1, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  # lda_in * 2\n"   \
      "  slli t1, a3, 3\n"   \
      "  # stride_out * 1 = 16 * 4 bytes = 64\n"   \
      "  li t2, 64\n"   \
      "  # stride_out * 2 = 16 * 8 bytes = 128\n"   \
      "  li t3, 128\n"   \
      "  \n"   \
      "  # Load 8 columns from the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  add a2, a6, t1\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v5, (a2)\n"   \
      "  vle32.v v6, (a5)\n"   \
      "  vle32.v v7, (a6)\n"   \
      "  vle32.v v8, (a7)\n"   \
      "  # Update the pointer to the next block of 8 columns\n"   \
      "  add a2, a7, t0\n"   \
      "  \n"   \
      "  # There might be junk at the end of the vectors, but we don't care,\n"   \
      "  # as it won't be written out\n"    \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"    \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  # v3 = [a20, a21, a22, a23, a24, a25, a26, a27]\n"   \
      "  # v4 = [a30, a31, a32, a33, a34, a35, a36, a37]\n"   \
      "  # v5 = [a40, a41, a42, a43, a44, a45, a46, a47]\n"   \
      "  # v6 = [a50, a51, a52, a53, a54, a55, a56, a57]\n"   \
      "  # v7 = [a60, a61, a62, a63, a64, a65, a66, a67]\n"   \
      "  # v8 = [a70, a71, a72, a73, a74, a75, a76, a77]\n"   \
      "  \n"   \
      "  \n"   \
      VZIPEVEN(9, 1, 2) "\n"   \
      VZIPODD(10, 1, 2) "\n"   \
      VZIPEVEN(11, 3, 4) "\n"   \
      VZIPODD(12, 3, 4) "\n"   \
      VZIPEVEN(13, 5, 6) "\n"   \
      VZIPODD(14, 5, 6) "\n"   \
      VZIPEVEN(15, 7, 8) "\n"   \
      VZIPODD(16, 7, 8) "\n"   \
      "  # v9 =  [a00, a10, a02, a12, a04, a14, a06, a16]\n"   \
      "  # v10 = [a01, a11, a03, a13, a05, a15, a07, a17]\n"   \
      "  # v11 = [a20, a30, a22, a32, a24, a34, a26, a36]\n"   \
      "  # v12 = [a21, a31, a23, a33, a25, a35, a27, a37]\n"   \
      "  # v13 = [a40, a50, a42, a52, a44, a54, a46, a56]\n"   \
      "  # v14 = [a41, a51, a43, a53, a45, a55, a47, a57]\n"   \
      "  # v15 = [a60, a70, a62, a72, a64, a74, a66, a76]\n"   \
      "  # v16 = [a61, a71, a63, a73, a65, a75, a67, a77]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIPEVEN(1, 9, 11) "\n"   \
      VZIPEVEN(2, 10, 12) "\n"   \
      VZIPODD(3, 9, 11) "\n"   \
      VZIPODD(4, 10, 12) "\n"   \
      VZIPEVEN(5, 13, 15) "\n"   \
      VZIPEVEN(6, 14, 16) "\n"   \
      VZIPODD(7, 13, 15) "\n"   \
      VZIPODD(8, 14, 16) "\n"   \
      "  # v1 = [a00, a10, a20, a30, a04, a14, a24, a34]\n"   \
      "  # v2 = [a01, a11, a21, a31, a05, a15, a25, a35]\n"   \
      "  # v3 = [a02, a12, a22, a32, a06, a16, a26, a36]\n"   \
      "  # v4 = [a03, a13, a23, a33, a07, a17, a27, a37]\n"   \
      "  # v5 = [a40, a50, a60, a70, a44, a54, a64, a74]\n"   \
      "  # v6 = [a41, a51, a61, a71, a45, a55, a65, a75]\n"   \
      "  # v7 = [a42, a52, a62, a72, a46, a56, a66, a76]\n"   \
      "  # v8 = [a43, a53, a63, a73, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Now deal with 2 * 64 = 128 bits at a time\n"   \
      "  li a5, 3\n"   \
      "  vmv.s.x v0, a5\n"   \
      "  \n"   \
      "  \n"   \
      "  vslidedown.vi v9, v1, 2\n"   \
      "  vslidedown.vi v10, v2, 2\n"   \
      "  vslidedown.vi v11, v3, 2\n"   \
      "  vslidedown.vi v12, v4, 2\n"   \
      "  vslideup.vi v1, v5, 2\n"   \
      "  vslideup.vi v2, v6, 2\n"   \
      "  vslideup.vi v3, v7, 2\n"   \
      "  vslideup.vi v4, v8, 2\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v9 = [a04, a14, a24, a34,   ?,   ?,   ?,   ?]\n"   \
      "  # v10 = [a05, a15, a25, a35,   ?,   ?,   ?,   ?]\n"   \
      "  # v11 = [a06, a16, a26, a36,   ?,   ?,   ?,   ?]\n"   \
      "  # v12 = [a07, a17, a27, a37,   ?,   ?,   ?,   ?]\n"   \
      "  \n"   \
      "  vmerge.vvm v5, v5, v9, v0\n"   \
      "  vmerge.vvm v6, v6, v10, v0\n"   \
      "  vmerge.vvm v7, v7, v11, v0\n"   \
      "  vmerge.vvm v8, v8, v12, v0\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v5 = [a04, a14, a24, a34, a44, a54, a64, a74]\n"   \
      "  # v6 = [a05, a15, a25, a35, a45, a55, a65, a75]\n"   \
      "  # v7 = [a06, a16, a26, a36, a46, a56, a66, a76]\n"   \
      "  # v8 = [a07, a17, a27, a37, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  # We only write out the same number of columns as there were rows\n"   \
      "  addi a3, a1, -2\n"    \
      "  beqz a3, 1f\n"        \
      "  vse64.v v3, (a6)\n"   \
      "  vse64.v v4, (a7)\n"   \
      "  addi a3, a3, -2\n"    \
      "  beqz a3, 1f\n"        \
      "  \n"   \
      "  add a0, a6, t3\n"   \
      "  add a5, a0, t2\n"   \
      "  add a6, a0, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v5, (a0)\n"   \
      "  vse64.v v6, (a5)\n"   \
      "  addi a3, a3, -2\n"    \
      "  beqz a3, 1f\n"        \
      "  vse64.v v7, (a6)\n"   \
      "  vse64.v v8, (a7)\n"
      "  1:\n"
      "  # Move the output pointer forward by one vector register's \n"   \
      "  # worth of bytes\n"   \
      "  addi a4, a4, 32\n"   \
      "  # Reset the vector length to the number of remaining rows\n"   \
      "  vsetvli zero, a1, e32, m1, ta, ma\n"   \
      "  # Load 8 columns from the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  add a2, a6, t1\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v5, (a2)\n"   \
      "  vle32.v v6, (a5)\n"   \
      "  vle32.v v7, (a6)\n"   \
      "  vle32.v v8, (a7)\n"   \
      "  # Update the pointer to the next block of 8 columns\n"   \
      "  add a2, a7, t0\n"   \
      "  \n"   \
      "  # There might be junk at the end of the vectors, but we don't care,\n"   \
      "  # as it won't be written out\n"    \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"    \
      "  # v1 = [a08, a09, a0a, a0b, a0c, a0d, a0e, a0f]\n"   \
      "  # v2 = [a18, a19, a1a, a1b, a1c, a1d, a1e, a1f]\n"   \
      "  # v3 = [a28, a29, a2a, a2b, a2c, a2d, a2e, a2f]\n"   \
      "  # v4 = [a38, a39, a3a, a3b, a3c, a3d, a3e, a3f]\n"   \
      "  # v5 = [a48, a49, a4a, a4b, a4c, a4d, a4e, a4f]\n"   \
      "  # v6 = [a58, a59, a5a, a5b, a5c, a5d, a5e, a5f]\n"   \
      "  # v7 = [a68, a69, a6a, a6b, a6c, a6d, a6e, a6f]\n"   \
      "  # v8 = [a78, a79, a7a, a7b, a7c, a7d, a7e, a7f]\n"   \
      "  \n"   \
      "  \n"   \
      VZIPEVEN(9, 1, 2) "\n"   \
      VZIPODD(10, 1, 2) "\n"   \
      VZIPEVEN(11, 3, 4) "\n"   \
      VZIPODD(12, 3, 4) "\n"   \
      VZIPEVEN(13, 5, 6) "\n"   \
      VZIPODD(14, 5, 6) "\n"   \
      VZIPEVEN(15, 7, 8) "\n"   \
      VZIPODD(16, 7, 8) "\n"   \
      "  # v9 =  [a08, a18, a0a, a1a, a0c, a1c, a0e, a1e]\n"   \
      "  # v10 = [a09, a19, a0b, a1b, a0d, a1d, a0f, a1f]\n"   \
      "  # v11 = [a28, a38, a2a, a3a, a2c, a3c, a2e, a3e]\n"   \
      "  # v12 = [a29, a39, a2b, a3b, a2d, a3d, a2f, a3f]\n"   \
      "  # v13 = [a48, a58, a4a, a5a, a4c, a5c, a4e, a5e]\n"   \
      "  # v14 = [a49, a59, a4b, a5b, a4d, a5d, a4f, a5f]\n"   \
      "  # v15 = [a68, a78, a6a, a7a, a6c, a7c, a6e, a7e]\n"   \
      "  # v16 = [a69, a79, a6b, a7b, a6d, a7d, a6f, a7f]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIPEVEN(1, 9, 11) "\n"   \
      VZIPEVEN(2, 10, 12) "\n"   \
      VZIPODD(3, 9, 11) "\n"   \
      VZIPODD(4, 10, 12) "\n"   \
      VZIPEVEN(5, 13, 15) "\n"   \
      VZIPEVEN(6, 14, 16) "\n"   \
      VZIPODD(7, 13, 15) "\n"   \
      VZIPODD(8, 14, 16) "\n"   \
      "  # v1 = [a08, a18, a28, a38, a0c, a1c, a2c, a3c]\n"   \
      "  # v2 = [a09, a19, a29, a39, a0d, a1d, a2d, a3d]\n"   \
      "  # v3 = [a0a, a1a, a2a, a3a, a0e, a1e, a2e, a3e]\n"   \
      "  # v4 = [a0b, a1b, a2b, a3b, a0f, a1f, a2f, a3f]\n"   \
      "  # v5 = [a48, a58, a68, a78, a4c, a5c, a6c, a7c]\n"   \
      "  # v6 = [a49, a59, a69, a79, a4d, a5d, a6d, a7d]\n"   \
      "  # v7 = [a4a, a5a, a6a, a7a, a4e, a5e, a6e, a7e]\n"   \
      "  # v8 = [a4b, a5b, a6b, a7b, a4f, a5f, a6f, a7f]\n"   \
      "  \n"   \
      "  # Now deal with 2 * 64 = 128 bits at a time\n"   \
      "  li a5, 3\n"   \
      "  vmv.s.x v0, a5\n"   \
      "  \n"   \
      "  \n"   \
      "  vslidedown.vi v9, v1, 2\n"   \
      "  vslidedown.vi v10, v2, 2\n"   \
      "  vslidedown.vi v11, v3, 2\n"   \
      "  vslidedown.vi v12, v4, 2\n"   \
      "  vslideup.vi v1, v5, 2\n"   \
      "  vslideup.vi v2, v6, 2\n"   \
      "  vslideup.vi v3, v7, 2\n"   \
      "  vslideup.vi v4, v8, 2\n"   \
      "  # v1 = [a08, a18, a28, a38, a48, a58, a68, a78]\n"   \
      "  # v2 = [a09, a19, a29, a39, a49, a59, a69, a79]\n"   \
      "  # v3 = [a0a, a1a, a2a, a3a, a4a, a5a, a6a, a7a]\n"   \
      "  # v4 = [a0b, a1b, a2b, a3b, a4b, a5b, a6b, a7b]\n"   \
      "  # v9 =  [a0c, a1c, a2c, a3c,   ?,   ?,   ?,   ?]\n"   \
      "  # v10 = [a0d, a1d, a2d, a3d,   ?,   ?,   ?,   ?]\n"   \
      "  # v11 = [a0e, a1e, a2e, a3e,   ?,   ?,   ?,   ?]\n"   \
      "  # v12 = [a0f, a1f, a2f, a3f,   ?,   ?,   ?,   ?]\n"   \
      "  \n"   \
      "  vmerge.vvm v5, v5, v9, v0\n"   \
      "  vmerge.vvm v6, v6, v10, v0\n"   \
      "  vmerge.vvm v7, v7, v11, v0\n"   \
      "  vmerge.vvm v8, v8, v12, v0\n"   \
      "  # v1 = [a08, a18, a28, a38, a48, a58, a68, a78]\n"   \
      "  # v2 = [a09, a19, a29, a39, a49, a59, a69, a79]\n"   \
      "  # v3 = [a0a, a1a, a2a, a3a, a4a, a5a, a6a, a7a]\n"   \
      "  # v4 = [a0b, a1b, a2b, a3b, a4b, a5b, a6b, a7b]\n"   \
      "  # v5 = [a0c, a1c, a2c, a3c, a4c, a5c, a6c, a7c]\n"   \
      "  # v6 = [a0d, a1d, a2d, a3d, a4d, a5d, a6d, a7d]\n"   \
      "  # v7 = [a0e, a1e, a2e, a3e, a4e, a5e, a6e, a7e]\n"   \
      "  # v8 = [a0f, a1f, a2f, a3f, a4f, a5f, a6f, a7f]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  # We only write out the same number of columns as there were rows\n"   \
      "  addi a3, a1, -2\n"    \
      "  beqz a3, 2f\n"        \
      "  vse64.v v3, (a6)\n"   \
      "  vse64.v v4, (a7)\n"   \
      "  addi a3, a3, -2\n"    \
      "  beqz a3, 2f\n"        \
      "  \n"   \
      "  add a0, a6, t3\n"   \
      "  add a5, a0, t2\n"   \
      "  add a6, a0, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v5, (a0)\n"   \
      "  vse64.v v6, (a5)\n"   \
      "  addi a3, a3, -2\n"    \
      "  beqz a1, 2f\n"        \
      "  vse64.v v7, (a6)\n"   \
      "  vse64.v v8, (a7)\n"   \
      "  2:\n"
      // Output operands
      :
      // Input operands
      : [m_in] "r" (m),
        [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3",
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15", "v16"
  );
}

static inline __attribute__((always_inline))
void transpose_2x8x4(IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Conceptually, this is just the same as 8x8 transpose, as we write out the
  // data in the same format
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  # lda_in * 2\n"   \
      "  slli t1, a3, 3\n"   \
      "  # stride_out * 1 = 8 * 4 bytes = 32\n"   \
      "  li t2, 32\n"   \
      "  # stride_out * 2 = 8 * 8 bytes = 64\n"   \
      "  li t3, 64\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  add a2, a6, t1\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v5, (a2)\n"   \
      "  vle32.v v6, (a5)\n"   \
      "  vle32.v v7, (a6)\n"   \
      "  vle32.v v8, (a7)\n"   \
      "  \n"   \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  # v3 = [a20, a21, a22, a23, a24, a25, a26, a27]\n"   \
      "  # v4 = [a30, a31, a32, a33, a34, a35, a36, a37]\n"   \
      "  # v5 = [a40, a41, a42, a43, a44, a45, a46, a47]\n"   \
      "  # v6 = [a50, a51, a52, a53, a54, a55, a56, a57]\n"   \
      "  # v7 = [a60, a61, a62, a63, a64, a65, a66, a67]\n"   \
      "  # v8 = [a70, a71, a72, a73, a74, a75, a76, a77]\n"   \
      "  \n"   \
      "  \n"   \
      VZIPEVEN(9, 1, 2) "\n"   \
      VZIPODD(10, 1, 2) "\n"   \
      VZIPEVEN(11, 3, 4) "\n"   \
      VZIPODD(12, 3, 4) "\n"   \
      VZIPEVEN(13, 5, 6) "\n"   \
      VZIPODD(14, 5, 6) "\n"   \
      VZIPEVEN(15, 7, 8) "\n"   \
      VZIPODD(16, 7, 8) "\n"   \
      "  # v9 =  [a00, a10, a02, a12, a04, a14, a06, a16]\n"   \
      "  # v10 = [a01, a11, a03, a13, a05, a15, a07, a17]\n"   \
      "  # v11 = [a20, a30, a22, a32, a24, a34, a26, a36]\n"   \
      "  # v12 = [a21, a31, a23, a33, a25, a35, a27, a37]\n"   \
      "  # v13 = [a40, a50, a42, a52, a44, a54, a46, a56]\n"   \
      "  # v14 = [a41, a51, a43, a53, a45, a55, a47, a57]\n"   \
      "  # v15 = [a60, a70, a62, a72, a64, a74, a66, a76]\n"   \
      "  # v16 = [a61, a71, a63, a73, a65, a75, a67, a77]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIPEVEN(1, 9, 11) "\n"   \
      VZIPEVEN(2, 10, 12) "\n"   \
      VZIPODD(3, 9, 11) "\n"   \
      VZIPODD(4, 10, 12) "\n"   \
      VZIPEVEN(5, 13, 15) "\n"   \
      VZIPEVEN(6, 14, 16) "\n"   \
      VZIPODD(7, 13, 15) "\n"   \
      VZIPODD(8, 14, 16) "\n"   \
      "  # v1 = [a00, a10, a20, a30, a04, a14, a24, a34]\n"   \
      "  # v2 = [a01, a11, a21, a31, a05, a15, a25, a35]\n"   \
      "  # v3 = [a02, a12, a22, a32, a06, a16, a26, a36]\n"   \
      "  # v4 = [a03, a13, a23, a33, a07, a17, a27, a37]\n"   \
      "  # v5 = [a40, a50, a60, a70, a44, a54, a64, a74]\n"   \
      "  # v6 = [a41, a51, a61, a71, a45, a55, a65, a75]\n"   \
      "  # v7 = [a42, a52, a62, a72, a46, a56, a66, a76]\n"   \
      "  # v8 = [a43, a53, a63, a73, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Now deal with 2 * 64 = 128 bits at a time\n"   \
      "  li a5, 3\n"   \
      "  vmv.s.x v0, a5\n"   \
      "  \n"   \
      "  vslidedown.vi v9, v1, 2\n"   \
      "  vslidedown.vi v10, v2, 2\n"   \
      "  vslidedown.vi v11, v3, 2\n"   \
      "  vslidedown.vi v12, v4, 2\n"   \
      "  vslideup.vi v1, v5, 2\n"   \
      "  vslideup.vi v2, v6, 2\n"   \
      "  vslideup.vi v3, v7, 2\n"   \
      "  vslideup.vi v4, v8, 2\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v9 = [a04, a14, a24, a34,   ?,   ?,   ?,   ?]\n"   \
      "  # v10 = [a05, a15, a25, a35,   ?,   ?,   ?,   ?]\n"   \
      "  # v11 = [a06, a16, a26, a36,   ?,   ?,   ?,   ?]\n"   \
      "  # v12 = [a07, a17, a27, a37,   ?,   ?,   ?,   ?]\n"   \
      "  \n"   \
      "  vmerge.vvm v5, v5, v9, v0\n"   \
      "  vmerge.vvm v6, v6, v10, v0\n"   \
      "  vmerge.vvm v7, v7, v11, v0\n"   \
      "  vmerge.vvm v8, v8, v12, v0\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v5 = [a04, a14, a24, a34, a44, a54, a64, a74]\n"   \
      "  # v6 = [a05, a15, a25, a35, a45, a55, a65, a75]\n"   \
      "  # v7 = [a06, a16, a26, a36, a46, a56, a66, a76]\n"   \
      "  # v8 = [a07, a17, a27, a37, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  vse64.v v3, (a6)\n"   \
      "  vse64.v v4, (a7)\n"   \
      "  \n"   \
      "  add a4, a6, t3\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v5, (a4)\n"   \
      "  vse64.v v6, (a5)\n"   \
      "  vse64.v v7, (a6)\n"   \
      "  vse64.v v8, (a7)\n"
      // Output operands
      :
      // Input operands
      : [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3",
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}

static inline __attribute__((always_inline))
void transpose_2x8xm(int m, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Conceptually, this is the same as 2mx8 transpose
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a1, %[m_in]\n"     \
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  # We set the vector length to the number of remaining rows\n"   \
      "  vsetvli zero, a1, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  # lda_in * 2\n"   \
      "  slli t1, a3, 3\n"   \
      "  # stride_out * 1 = 8 * 4 bytes = 32\n"   \
      "  li t2, 32\n"   \
      "  # stride_out * 2 = 8 * 8 bytes = 64\n"   \
      "  li t3, 64\n"   \
      "  \n"   \
      "  # Load 8 columns from the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  add a2, a6, t1\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v5, (a2)\n"   \
      "  vle32.v v6, (a5)\n"   \
      "  vle32.v v7, (a6)\n"   \
      "  vle32.v v8, (a7)\n"   \
      "  \n"   \
      "  # There might be junk at the end of the vectors, but we don't care,\n"   \
      "  # as it won't be written out\n"    \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"    \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  # v3 = [a20, a21, a22, a23, a24, a25, a26, a27]\n"   \
      "  # v4 = [a30, a31, a32, a33, a34, a35, a36, a37]\n"   \
      "  # v5 = [a40, a41, a42, a43, a44, a45, a46, a47]\n"   \
      "  # v6 = [a50, a51, a52, a53, a54, a55, a56, a57]\n"   \
      "  # v7 = [a60, a61, a62, a63, a64, a65, a66, a67]\n"   \
      "  # v8 = [a70, a71, a72, a73, a74, a75, a76, a77]\n"   \
      "  \n"   \
      "  \n"   \
      VZIPEVEN(9, 1, 2) "\n"   \
      VZIPODD(10, 1, 2) "\n"   \
      VZIPEVEN(11, 3, 4) "\n"   \
      VZIPODD(12, 3, 4) "\n"   \
      VZIPEVEN(13, 5, 6) "\n"   \
      VZIPODD(14, 5, 6) "\n"   \
      VZIPEVEN(15, 7, 8) "\n"   \
      VZIPODD(16, 7, 8) "\n"   \
      "  # v9 =  [a00, a10, a02, a12, a04, a14, a06, a16]\n"   \
      "  # v10 = [a01, a11, a03, a13, a05, a15, a07, a17]\n"   \
      "  # v11 = [a20, a30, a22, a32, a24, a34, a26, a36]\n"   \
      "  # v12 = [a21, a31, a23, a33, a25, a35, a27, a37]\n"   \
      "  # v13 = [a40, a50, a42, a52, a44, a54, a46, a56]\n"   \
      "  # v14 = [a41, a51, a43, a53, a45, a55, a47, a57]\n"   \
      "  # v15 = [a60, a70, a62, a72, a64, a74, a66, a76]\n"   \
      "  # v16 = [a61, a71, a63, a73, a65, a75, a67, a77]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIPEVEN(1, 9, 11) "\n"   \
      VZIPEVEN(2, 10, 12) "\n"   \
      VZIPODD(3, 9, 11) "\n"   \
      VZIPODD(4, 10, 12) "\n"   \
      VZIPEVEN(5, 13, 15) "\n"   \
      VZIPEVEN(6, 14, 16) "\n"   \
      VZIPODD(7, 13, 15) "\n"   \
      VZIPODD(8, 14, 16) "\n"   \
      "  # v1 = [a00, a10, a20, a30, a04, a14, a24, a34]\n"   \
      "  # v2 = [a01, a11, a21, a31, a05, a15, a25, a35]\n"   \
      "  # v3 = [a02, a12, a22, a32, a06, a16, a26, a36]\n"   \
      "  # v4 = [a03, a13, a23, a33, a07, a17, a27, a37]\n"   \
      "  # v5 = [a40, a50, a60, a70, a44, a54, a64, a74]\n"   \
      "  # v6 = [a41, a51, a61, a71, a45, a55, a65, a75]\n"   \
      "  # v7 = [a42, a52, a62, a72, a46, a56, a66, a76]\n"   \
      "  # v8 = [a43, a53, a63, a73, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Now deal with 2 * 64 = 128 bits at a time\n"   \
      "  li a5, 3\n"   \
      "  vmv.s.x v0, a5\n"   \
      "  \n"   \
      "  \n"   \
      "  vslidedown.vi v9, v1, 2\n"   \
      "  vslidedown.vi v10, v2, 2\n"   \
      "  vslidedown.vi v11, v3, 2\n"   \
      "  vslidedown.vi v12, v4, 2\n"   \
      "  vslideup.vi v1, v5, 2\n"   \
      "  vslideup.vi v2, v6, 2\n"   \
      "  vslideup.vi v3, v7, 2\n"   \
      "  vslideup.vi v4, v8, 2\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v9 = [a04, a14, a24, a34,   ?,   ?,   ?,   ?]\n"   \
      "  # v10 = [a05, a15, a25, a35,   ?,   ?,   ?,   ?]\n"   \
      "  # v11 = [a06, a16, a26, a36,   ?,   ?,   ?,   ?]\n"   \
      "  # v12 = [a07, a17, a27, a37,   ?,   ?,   ?,   ?]\n"   \
      "  \n"   \
      "  vmerge.vvm v5, v5, v9, v0\n"   \
      "  vmerge.vvm v6, v6, v10, v0\n"   \
      "  vmerge.vvm v7, v7, v11, v0\n"   \
      "  vmerge.vvm v8, v8, v12, v0\n"   \
      "  # v1 = [a00, a10, a20, a30, a40, a50, a60, a70]\n"   \
      "  # v2 = [a01, a11, a21, a31, a41, a51, a61, a71]\n"   \
      "  # v3 = [a02, a12, a22, a32, a42, a52, a62, a72]\n"   \
      "  # v4 = [a03, a13, a23, a33, a43, a53, a63, a73]\n"   \
      "  # v5 = [a04, a14, a24, a34, a44, a54, a64, a74]\n"   \
      "  # v6 = [a05, a15, a25, a35, a45, a55, a65, a75]\n"   \
      "  # v7 = [a06, a16, a26, a36, a46, a56, a66, a76]\n"   \
      "  # v8 = [a07, a17, a27, a37, a47, a57, a67, a77]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  # We only write out the same number of columns as there were rows\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  vse64.v v2, (a5)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  vse64.v v3, (a6)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  vse64.v v4, (a7)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  \n"   \
      "  add a4, a6, t3\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v5, (a4)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  vse64.v v6, (a5)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  vse64.v v7, (a6)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"        \
      "  vse64.v v8, (a7)\n"   \
      "  1:\n"
      // Output operands
      :
      // Input operands
      : [m_in] "r" (m),
        [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3",
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}

static inline __attribute__((always_inline))
void transpose_2x4x4(IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Transpose 4 sets of 2x4 blocks, and write them out contiguously
  // This is a little more interesting, because we can store more than a single
  // row in a vector register, so this is not the same as an 8x4 transpose
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  # lda_in * 2\n"   \
      "  slli t1, a3, 3\n"   \
      "  # stride_out * 1 = 8 * 4 bytes = 32\n"   \
      "  li t2, 32\n"   \
      "  # stride_out * 2 = 8 * 8 bytes = 64\n"   \
      "  li t3, 64\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  # v3 = [a20, a21, a22, a23, a24, a25, a26, a27]\n"   \
      "  # v4 = [a30, a31, a32, a33, a34, a35, a36, a37]\n"   \
      "  \n"   \
      "  \n"   \
      VZIP2A(5, 1, 2) "\n"   \
      VZIP2B(6, 1, 2) "\n"   \
      VZIP2A(7, 3, 4) "\n"   \
      VZIP2B(8, 3, 4) "\n"   \
      "  # v5 = [a00, a10, a01, a11, a02, a12, a03, a13]\n"   \
      "  # v6 = [a04, a14, a05, a15, a06, a16, a07, a17]\n"   \
      "  # v7 = [a20, a30, a21, a31, a22, a32, a23, a33]\n"   \
      "  # v8 = [a24, a34, a25, a35, a26, a36, a27, a37]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIP2A(1, 5, 7) "\n"   \
      VZIP2B(2, 5, 7) "\n"   \
      VZIP2A(3, 6, 8) "\n"   \
      VZIP2B(4, 6, 8) "\n"   \
      "  # v1 = [a00, a10, a20, a30, a01, a11, a21, a31]\n"   \
      "  # v2 = [a02, a12, a22, a32, a03, a13, a23, a33]\n"   \
      "  # v3 = [a04, a14, a24, a34, a05, a15, a25, a35]\n"   \
      "  # v4 = [a06, a16, a26, a36, a07, a17, a27, a37]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  vse64.v v3, (a6)\n"   \
      "  vse64.v v4, (a7)\n"   \
      "  \n"   \
      // Output operands
      :
      // Input operands
      : [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3",
        "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}

static inline __attribute__((always_inline))
void transpose_2x4xm(int m, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Transpose m sets of 2x4 blocks, and write them out contiguously
  // This is a little more interesting, because we can store more than a single
  // row in a vector register, so this is not the same as an 8x4 transpose
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a1, %[m_in]\n"       \
      "  mv a2, %[a_in]\n"       \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  vsetvli zero, a1, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  # lda_in * 2\n"   \
      "  slli t1, a3, 3\n"   \
      "  # stride_out * 1 = 8 * 4 bytes = 32\n"   \
      "  li t2, 32\n"   \
      "  # stride_out * 2 = 8 * 8 bytes = 64\n"   \
      "  li t3, 64\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  add a6, a2, t1\n"   \
      "  add a7, a5, t1\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  # v3 = [a20, a21, a22, a23, a24, a25, a26, a27]\n"   \
      "  # v4 = [a30, a31, a32, a33, a34, a35, a36, a37]\n"   \
      "  \n"   \
      "  \n"   \
      VZIP2A(5, 1, 2) "\n"   \
      VZIP2B(6, 1, 2) "\n"   \
      VZIP2A(7, 3, 4) "\n"   \
      VZIP2B(8, 3, 4) "\n"   \
      "  # v5 = [a00, a10, a01, a11, a02, a12, a03, a13]\n"   \
      "  # v6 = [a04, a14, a05, a15, a06, a16, a07, a17]\n"   \
      "  # v7 = [a20, a30, a21, a31, a22, a32, a23, a33]\n"   \
      "  # v8 = [a24, a34, a25, a35, a26, a36, a27, a37]\n"   \
      "  \n"   \
      "  vsetivli zero, 4, e64, m1, ta, ma\n"   \
      VZIP2A(1, 5, 7) "\n"   \
      VZIP2B(2, 5, 7) "\n"   \
      VZIP2A(3, 6, 8) "\n"   \
      VZIP2B(4, 6, 8) "\n"   \
      "  # v1 = [a00, a10, a20, a30, a01, a11, a21, a31]\n"   \
      "  # v2 = [a02, a12, a22, a32, a03, a13, a23, a33]\n"   \
      "  # v3 = [a04, a14, a24, a34, a05, a15, a25, a35]\n"   \
      "  # v4 = [a06, a16, a26, a36, a07, a17, a27, a37]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t2\n"   \
      "  add a6, a4, t3\n"   \
      "  add a7, a5, t3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  addi a1, a1, -2\n"   \
      "  beqz a1, 1f\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  addi a1, a1, -2\n"   \
      "  beqz a1, 1f\n"   \
      "  vse64.v v3, (a6)\n"   \
      "  addi a1, a1, -2\n"   \
      "  beqz a1, 1f\n"   \
      "  vse64.v v4, (a7)\n"   \
      "  1:\n"   \
      "  \n"   \
      // Output operands
      :
      // Input operands
      : [m_in] "r" (m),
        [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3",
        "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}

static inline __attribute__((always_inline))
void transpose_2x2x4(IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Transpose 4 sets of 2x2 blocks, and write them out contiguously
  // This is a little more interesting, because we can store more than a single
  // row in a vector register, so this is not the same as an 8x2 transpose
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  add a5, a2, t0\n"   \
      "  # stride_out * 1 = 8 * 4 bytes = 32\n"   \
      "  li t1, 32\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  \n"   \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  \n"   \
      "  \n"   \
      VZIP2A(3, 1, 2) "\n"   \
      VZIP2B(4, 1, 2) "\n"   \
      "  # v3 = [a00, a10, a01, a11, a02, a12, a03, a13]\n"   \
      "  # v4 = [a04, a14, a05, a15, a16, a16, a07, a17]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, t1\n"   \
      "  vse32.v v3, (a4)\n"   \
      "  vse32.v v4, (a5)\n"   \
      "  \n"   \
      // Output operands
      :
      // Input operands
      : [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a2", "a3", "a4", "a5",
        "t0", "t1",
        "v1", "v2", "v3", "v4"
  );
}

static inline __attribute__((always_inline))
void transpose_2x2xm(int m, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  // Transpose m sets of 2x4 blocks, and write them out contiguously
  // This is a little more interesting, because we can store more than a single
  // row in a vector register, so this is not the same as an 8x4 transpose
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a1, %[m_in]\n"       \
      "  mv a2, %[a_in]\n"       \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  vsetvli zero, a1, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # lda_in * 1\n"   \
      "  slli t0, a3, 2\n"   \
      "  # stride_out * 1 = 8 * 4 bytes = 32\n"   \
      "  li t2, 32\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  add a5, a2, t0\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  \n"   \
      "  vsetivli zero, 8, e32, m1, ta, ma\n"   \
      "  # v1 = [a00, a01, a02, a03, a04, a05, a06, a07]\n"   \
      "  # v2 = [a10, a11, a12, a13, a14, a15, a16, a17]\n"   \
      "  \n"   \
      "  \n"   \
      VZIP2A(4, 1, 2) "\n"   \
      VZIP2B(5, 1, 2) "\n"   \
      "  # v3 = [a00, a10, a01, a11, a02, a12, a03, a13]\n"   \
      "  # v4 = [a04, a14, a05, a15, a16, a16, a07, a17]\n"   \
      "  \n"   \
      "  # Per 2 input rows we have 4 output elements\n"   \
      "  slli a6, a1, 1\n"   \
      "  vsetvli zero, a6, e32, m2, ta, ma\n"   \
      "  # Write out the result\n"   \
      "  vse32.v v4, (a4)\n"   \
      "  \n"   \
      // Output operands
      :
      // Input operands
      : [m_in] "r" (m),
        [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3",
        "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}

// clang-format on
int CNAME(BLASLONG m, BLASLONG n, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  IFLOAT *col_a_ptr = a;
  IFLOAT *b_ptr = b;
  for (int32_t nn = 0; nn < n - 15; nn += 16) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 16 * lda;
    for (int32_t mm = 0; mm < m - 7; mm += 8) {
      transpose_2x16x4(tiled_col_a_ptr, lda, b_ptr);
      tiled_col_a_ptr += 8;
      b_ptr += 128;
    }
    // Deal with a tail of rows. We don't do any padding in this routine
    int32_t num_tail_rows = m & 6; // Even tail rows 6 = 0x110
    if (num_tail_rows > 0) {
      transpose_2x16xm(num_tail_rows, tiled_col_a_ptr, lda, b_ptr);
      b_ptr += num_tail_rows * 16;
      tiled_col_a_ptr += num_tail_rows;
    }

    if (m & 1) {
      // Copy a single row of 16 from strided to contiguous
      for (int i = 0; i < 16; ++i) {
        *b_ptr++ = tiled_col_a_ptr[i * lda];
      }
    }
  }

  if (n & 8) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 8 * lda;
    for (int32_t mm = 0; mm < m - 7; mm += 8) {
      transpose_2x8x4(tiled_col_a_ptr, lda, b_ptr);
      tiled_col_a_ptr += 8;
      b_ptr += 64;
    }

    // Deal with a tail of rows. We don't do any padding in this routine
    int32_t num_tail_rows = m & 6; // Even tail rows 6 = 0x110
    if (num_tail_rows > 0) {
      transpose_2x8xm(num_tail_rows, tiled_col_a_ptr, lda, b_ptr);
      b_ptr += num_tail_rows * 8;
      tiled_col_a_ptr += num_tail_rows;
    }

    if (m & 1) {
      // Copy a single row of 8 from strided to contiguous
      for (int i = 0; i < 8; ++i) {
        *b_ptr++ = tiled_col_a_ptr[i * lda];
      }
    }
  }

  if (n & 4) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 4 * lda;
    for (int32_t mm = 0; mm < m - 7; mm += 8) {
      transpose_2x4x4(tiled_col_a_ptr, lda, b_ptr);
      tiled_col_a_ptr += 8;
      b_ptr += 32;
    }

    // Deal with a tail of rows. We don't do any padding in this routine
    int32_t num_tail_rows = m & 6; // Even tail rows 6 = 0x110
    if (num_tail_rows > 0) {
      transpose_2x4xm(num_tail_rows, tiled_col_a_ptr, lda, b_ptr);
      b_ptr += num_tail_rows * 4;
      tiled_col_a_ptr += num_tail_rows;
    }

    if (m & 1) {
      // Copy a single row of 4 from strided to contiguous
      for (int i = 0; i < 4; ++i) {
        *b_ptr++ = tiled_col_a_ptr[i * lda];
      }
    }
  }

  if (n & 2) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 2 * lda;
    for (int32_t mm = 0; mm < m - 7; mm += 8) {
      transpose_2x2x4(tiled_col_a_ptr, lda, b_ptr);
      tiled_col_a_ptr += 8;
      b_ptr += 16;
    }

    // Deal with a tail of rows. We don't do any padding in this routine
    int32_t num_tail_rows = m & 6; // Even tail rows 6 = 0x110
    if (num_tail_rows > 0) {
      transpose_2x2xm(num_tail_rows, tiled_col_a_ptr, lda, b_ptr);
      b_ptr += num_tail_rows * 2;
      tiled_col_a_ptr += num_tail_rows;
    }

    if (m & 1) {
      for (int i = 0; i < 2; ++i) {
        *b_ptr++ = tiled_col_a_ptr[i * lda];
      }
    }
  }

  if (n & 1) {
    for (int i = 0; i < m; ++i) {
      b_ptr[i] = col_a_ptr[i];
    }
  }

  return 0;
}
