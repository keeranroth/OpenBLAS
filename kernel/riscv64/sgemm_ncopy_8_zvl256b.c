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

// clang-format off
static inline __attribute__((always_inline))
void transpose_8x8(IFLOAT *a, BLASLONG lda, IFLOAT *b) {
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
void transpose_mx8(BLASLONG m, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
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
      "  vse64.v v8, (a7)\n"
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
void transpose_4x4(IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  # Set the vector length\n"   \
      "  vsetivli zero, 4, e32, m1, ta, ma\n"   \
      "  # Set the number of bytes in lda\n"   \
      "  slli a3, a3, 2\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  add a5, a2, a3\n"   \
      "  add a6, a5, a3\n"   \
      "  add a7, a6, a3\n"   \
      "  # stride_out = 4 * 4 bytes = 16\n"   \
      "  li a3, 16\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  \n"   \
      "  # v1 = [a00, a01, a02, a03]\n"   \
      "  # v2 = [a10, a11, a12, a13]\n"   \
      "  # v3 = [a20, a21, a22, a23]\n"   \
      "  # v4 = [a30, a31, a32, a33]\n"   \
      "  \n"   \
      VZIPEVEN(5, 1, 2) "\n"   \
      VZIPODD(6, 1, 2) "\n"   \
      VZIPEVEN(7, 3, 4) "\n"   \
      VZIPODD(8, 3, 4) "\n"   \
      "  \n"   \
      "  # v5 = [a00, a10, a02, a12]\n"   \
      "  # v6 = [a01, a11, a03, a13]\n"   \
      "  # v7 = [a20, a30, a22, a32]\n"   \
      "  # v8 = [a21, a31, a23, a33]\n"   \
      "  \n"   \
      "  vsetivli zero, 2, e64, m1, ta, ma\n"   \
      "  \n"   \
      VZIPEVEN(1, 5, 7) "\n"   \
      VZIPEVEN(2, 6, 8) "\n"   \
      VZIPODD(3, 5, 7) "\n"   \
      VZIPODD(4, 6, 8) "\n"   \
      "  \n"   \
      "  # v1 = [a00, a10, a20, a30]\n"   \
      "  # v2 = [a01, a11, a21, a31]\n"   \
      "  # v3 = [a02, a12, a22, a23]\n"   \
      "  # v4 = [a03, a13, a23, a33]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, a3\n"   \
      "  add a6, a5, a3\n"   \
      "  add a7, a6, a3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  vse64.v v3, (a6)\n"   \
      "  vse64.v v4, (a7)\n"
      // Output operands
      :
      // Input operands
      : [a_in] "r" (a),
        [lda_in] "r" (lda),
        [b_in] "r" (b)
      // Clobbers
      : "a2", "a3", "a4", "a5", "a6", "a7",
        "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}

static inline __attribute__((always_inline))
void transpose_mx4(BLASLONG m, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  asm volatile(
      // We can't assume that the input registers haven't been moved around
      // before this, so we first load things into the right registers
      "  mv a1, %[m_in]\n"     \
      "  mv a2, %[a_in]\n"     \
      "  mv a3, %[lda_in]\n"     \
      "  mv a4, %[b_in]\n"       \
      "  # Set the vector length\n"   \
      "  vsetvli zero, a1, e32, m1, ta, ma\n"   \
      "  # Set the number of bytes in lda\n"   \
      "  slli a3, a3, 2\n"   \
      "  \n"   \
      "  # Load the matrix\n"   \
      "  add a5, a2, a3\n"   \
      "  add a6, a5, a3\n"   \
      "  add a7, a6, a3\n"   \
      "  # stride_out = 4 * 4 bytes = 16\n"   \
      "  li a3, 16\n"   \
      "  vle32.v v1, (a2)\n"   \
      "  vle32.v v2, (a5)\n"   \
      "  vle32.v v3, (a6)\n"   \
      "  vle32.v v4, (a7)\n"   \
      "  # The vector tails may have junk, but we don't care about it. We push\n"   \
      "  # the junk further down the vector registers, and only write out the ones\n"   \
      "  #that are full of result data\n"   \
      "  vsetivli zero, 4, e32, m1, ta, ma\n"   \
      "  \n"   \
      "  # v1 = [a00, a01, a02, a03]\n"   \
      "  # v2 = [a10, a11, a12, a13]\n"   \
      "  # v3 = [a20, a21, a22, a23]\n"   \
      "  # v4 = [a30, a31, a32, a33]\n"   \
      "  \n"   \
      VZIPEVEN(5, 1, 2) "\n"   \
      VZIPODD(6, 1, 2) "\n"   \
      VZIPEVEN(7, 3, 4) "\n"   \
      VZIPODD(8, 3, 4) "\n"   \
      "  \n"   \
      "  # v5 = [a00, a10, a02, a12]\n"   \
      "  # v6 = [a01, a11, a03, a13]\n"   \
      "  # v7 = [a20, a30, a22, a32]\n"   \
      "  # v8 = [a21, a31, a23, a33]\n"   \
      "  \n"   \
      "  vsetivli zero, 2, e64, m1, ta, ma\n"   \
      "  \n"   \
      VZIPEVEN(1, 5, 7) "\n"   \
      VZIPEVEN(2, 6, 8) "\n"   \
      VZIPODD(3, 5, 7) "\n"   \
      VZIPODD(4, 6, 8) "\n"   \
      "  \n"   \
      "  # v1 = [a00, a10, a20, a30]\n"   \
      "  # v2 = [a01, a11, a21, a31]\n"   \
      "  # v3 = [a02, a12, a22, a23]\n"   \
      "  # v4 = [a03, a13, a23, a33]\n"   \
      "  \n"   \
      "  # Write out the result\n"   \
      "  add a5, a4, a3\n"   \
      "  add a6, a5, a3\n"   \
      "  add a7, a6, a3\n"   \
      "  vse64.v v1, (a4)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"   \
      "  vse64.v v2, (a5)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"   \
      "  vse64.v v3, (a6)\n"   \
      "  addi a1, a1, -1\n"    \
      "  beqz a1, 1f\n"   \
      "  vse64.v v4, (a7)\n"
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
        "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8"
  );
}
// clang-format on

int CNAME(BLASLONG m, BLASLONG n, IFLOAT *a, BLASLONG lda, IFLOAT *b) {
  IFLOAT *col_a_ptr = a;
  IFLOAT *b_ptr = b;
  for (int32_t nn = 0; nn < n - 7; nn += 8) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 8 * lda;
    for (int32_t mm = 0; mm < m - 7; mm += 8) {
      transpose_8x8(tiled_col_a_ptr, lda, b_ptr);
      tiled_col_a_ptr += 8;
      b_ptr += 64;
    }
    // Deal with a tail of rows. We don't do any padding in this routine
    int32_t num_tail_rows = m & 7;
    if (num_tail_rows > 0) {
      transpose_mx8(num_tail_rows, tiled_col_a_ptr, lda, b_ptr);
      b_ptr += num_tail_rows * 8;
    }
  }

  // Now deal with the 4x4, 2x2 and 1x1 cases
  if (n & 4) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 4 * lda;
    for (int32_t mm = 0; mm < m - 3; mm += 4) {
      transpose_4x4(tiled_col_a_ptr, lda, b_ptr);
      tiled_col_a_ptr += 4;
      b_ptr += 16;
    }

    int32_t num_tail_rows = m & 3;
    if (num_tail_rows > 0) {
      transpose_mx4(num_tail_rows, tiled_col_a_ptr, lda, b_ptr);
      b_ptr += num_tail_rows * 4;
    }
  }

  if (n & 2) {
    IFLOAT *tiled_col_a_ptr = col_a_ptr;
    col_a_ptr += 2 * lda;
    for (int32_t mm = 0; mm < m - 1; mm += 2) {
      *b_ptr++ = tiled_col_a_ptr[0];
      *b_ptr++ = tiled_col_a_ptr[0 + lda];
      *b_ptr++ = tiled_col_a_ptr[1];
      *b_ptr++ = tiled_col_a_ptr[1 + lda];
      tiled_col_a_ptr += 2;
    }

    if (m & 1) {
      *b_ptr++ = tiled_col_a_ptr[0];
      *b_ptr++ = tiled_col_a_ptr[lda];
    }
  }

  if (n & 1) {
    for (int32_t mm = 0; mm < m; ++mm) {
      b_ptr[mm] = col_a_ptr[mm];
    }
  }
  return 0;
}