#pragma once

//
// The code in the RS namespace provides Reed-Solomon based error correction.
// The code is taken from https://github.com/mersinvald/Reed-Solomon.
// The LICENSE of the code is copied below:
//
// Copyright © 2015 Mike Lubinets, github.com/mersinvald
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation files
// (the “Software”), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// The GGWave implementation is in the GGWave namespace at the bottom of this file.
//

#include <assert.h>

namespace RS {

struct Poly {
    Poly()
        : length(0), _memory(NULL) {}

    Poly(uint8_t id, uint16_t offset, uint8_t size) \
        : length(0), _id(id), _size(size), _offset(offset), _memory(NULL) {}

    /* @brief Append number at the end of polynomial
     * @param num - number to append
     * @return false if polynomial can't be stretched */
    inline bool Append(uint8_t num) {
        assert(length < _size);
        ptr()[length++] = num;
        return true;
    }

    /* @brief Polynomial initialization */
    inline void Init(uint8_t id, uint16_t offset, uint8_t size, uint8_t** memory_ptr) {
        this->_id     = id;
        this->_offset = offset;
        this->_size   = size;
        this->length  = 0;
        this->_memory = memory_ptr;
    }

    /* @brief Polynomial memory zeroing */
    inline void Reset() {
        memset((void*)ptr(), 0, this->_size);
    }

    /* @brief Copy polynomial to memory
     * @param src    - source byte-sequence
     * @param size   - size of polynomial
     * @param offset - write offset */
    inline void Set(const uint8_t* src, uint8_t len, uint8_t offset = 0) {
        assert(src && len <= this->_size-offset);
        memcpy(ptr()+offset, src, len * sizeof(uint8_t));
        length = len + offset;
    }

#define poly_max(a, b) ((a > b) ? (a) : (b))

    inline void Copy(const Poly* src) {
        length = poly_max(length, src->length);
        Set(src->ptr(), length);
    }

    inline uint8_t& at(uint8_t i) const {
        assert(i < _size);
        return ptr()[i];
    }

    inline uint8_t id() const {
        return _id;
    }

    inline uint8_t size() const {
        return _size;
    }

    // Returns pointer to memory of this polynomial
    inline uint8_t* ptr() const {
        assert(_memory && *_memory);
        return (*_memory) + _offset;
    }

    uint8_t length;

protected:

    uint8_t   _id;
    uint8_t   _size;    // Size of reserved memory for this polynomial
    uint16_t  _offset;  // Offset in memory
    uint8_t** _memory;  // Pointer to pointer to memory
};

namespace gf {


/* GF tables pre-calculated for 0x11d primitive polynomial */

const uint8_t exp[512] PROGMEM = {
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26, 0x4c,
    0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9, 0x8f, 0x3, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc0, 0x9d,
    0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35, 0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23, 0x46,
    0x8c, 0x5, 0xa, 0x14, 0x28, 0x50, 0xa0, 0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1, 0x5f,
    0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc, 0x65, 0xca, 0x89, 0xf, 0x1e, 0x3c, 0x78, 0xf0, 0xfd,
    0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f, 0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2, 0xd9,
    0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88, 0xd, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce, 0x81,
    0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93, 0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc, 0x85,
    0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9, 0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54, 0xa8,
    0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa, 0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73, 0xe6,
    0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e, 0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff, 0xe3,
    0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4, 0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41, 0x82,
    0x19, 0x32, 0x64, 0xc8, 0x8d, 0x7, 0xe, 0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6, 0x51,
    0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef, 0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x9, 0x12,
    0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5, 0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0xb, 0x16, 0x2c,
    0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83, 0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x1, 0x2,
    0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26, 0x4c, 0x98,
    0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9, 0x8f, 0x3, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc0, 0x9d, 0x27,
    0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35, 0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23, 0x46, 0x8c,
    0x5, 0xa, 0x14, 0x28, 0x50, 0xa0, 0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1, 0x5f, 0xbe,
    0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc, 0x65, 0xca, 0x89, 0xf, 0x1e, 0x3c, 0x78, 0xf0, 0xfd, 0xe7,
    0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f, 0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2, 0xd9, 0xaf,
    0x43, 0x86, 0x11, 0x22, 0x44, 0x88, 0xd, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce, 0x81, 0x1f,
    0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93, 0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc, 0x85, 0x17,
    0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9, 0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54, 0xa8, 0x4d,
    0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa, 0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73, 0xe6, 0xd1,
    0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e, 0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff, 0xe3, 0xdb,
    0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4, 0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41, 0x82, 0x19,
    0x32, 0x64, 0xc8, 0x8d, 0x7, 0xe, 0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6, 0x51, 0xa2,
    0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef, 0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x9, 0x12, 0x24,
    0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5, 0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0xb, 0x16, 0x2c, 0x58,
    0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83, 0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x1, 0x2
};

const uint8_t log[256] PROGMEM = {
    0x0, 0x0, 0x1, 0x19, 0x2, 0x32, 0x1a, 0xc6, 0x3, 0xdf, 0x33, 0xee, 0x1b, 0x68, 0xc7, 0x4b, 0x4,
    0x64, 0xe0, 0xe, 0x34, 0x8d, 0xef, 0x81, 0x1c, 0xc1, 0x69, 0xf8, 0xc8, 0x8, 0x4c, 0x71, 0x5,
    0x8a, 0x65, 0x2f, 0xe1, 0x24, 0xf, 0x21, 0x35, 0x93, 0x8e, 0xda, 0xf0, 0x12, 0x82, 0x45, 0x1d,
    0xb5, 0xc2, 0x7d, 0x6a, 0x27, 0xf9, 0xb9, 0xc9, 0x9a, 0x9, 0x78, 0x4d, 0xe4, 0x72, 0xa6, 0x6,
    0xbf, 0x8b, 0x62, 0x66, 0xdd, 0x30, 0xfd, 0xe2, 0x98, 0x25, 0xb3, 0x10, 0x91, 0x22, 0x88, 0x36,
    0xd0, 0x94, 0xce, 0x8f, 0x96, 0xdb, 0xbd, 0xf1, 0xd2, 0x13, 0x5c, 0x83, 0x38, 0x46, 0x40, 0x1e,
    0x42, 0xb6, 0xa3, 0xc3, 0x48, 0x7e, 0x6e, 0x6b, 0x3a, 0x28, 0x54, 0xfa, 0x85, 0xba, 0x3d, 0xca,
    0x5e, 0x9b, 0x9f, 0xa, 0x15, 0x79, 0x2b, 0x4e, 0xd4, 0xe5, 0xac, 0x73, 0xf3, 0xa7, 0x57, 0x7,
    0x70, 0xc0, 0xf7, 0x8c, 0x80, 0x63, 0xd, 0x67, 0x4a, 0xde, 0xed, 0x31, 0xc5, 0xfe, 0x18, 0xe3,
    0xa5, 0x99, 0x77, 0x26, 0xb8, 0xb4, 0x7c, 0x11, 0x44, 0x92, 0xd9, 0x23, 0x20, 0x89, 0x2e, 0x37,
    0x3f, 0xd1, 0x5b, 0x95, 0xbc, 0xcf, 0xcd, 0x90, 0x87, 0x97, 0xb2, 0xdc, 0xfc, 0xbe, 0x61, 0xf2,
    0x56, 0xd3, 0xab, 0x14, 0x2a, 0x5d, 0x9e, 0x84, 0x3c, 0x39, 0x53, 0x47, 0x6d, 0x41, 0xa2, 0x1f,
    0x2d, 0x43, 0xd8, 0xb7, 0x7b, 0xa4, 0x76, 0xc4, 0x17, 0x49, 0xec, 0x7f, 0xc, 0x6f, 0xf6, 0x6c,
    0xa1, 0x3b, 0x52, 0x29, 0x9d, 0x55, 0xaa, 0xfb, 0x60, 0x86, 0xb1, 0xbb, 0xcc, 0x3e, 0x5a, 0xcb,
    0x59, 0x5f, 0xb0, 0x9c, 0xa9, 0xa0, 0x51, 0xb, 0xf5, 0x16, 0xeb, 0x7a, 0x75, 0x2c, 0xd7, 0x4f,
    0xae, 0xd5, 0xe9, 0xe6, 0xe7, 0xad, 0xe8, 0x74, 0xd6, 0xf4, 0xea, 0xa8, 0x50, 0x58, 0xaf
};



/* ################################
 * # OPERATIONS OVER GALUA FIELDS #
 * ################################ */

/* @brief Addition in Galua Fields
 * @param x - left operand
 * @param y - right operand
 * @return x + y */
inline uint8_t add(uint8_t x, uint8_t y) {
    return x^y;
}

/* ##### GF substraction ###### */
/* @brief Substraction in Galua Fields
 * @param x - left operand
 * @param y - right operand
 * @return x - y */
inline uint8_t sub(uint8_t x, uint8_t y) {
    return x^y;
}

/* @brief Multiplication in Galua Fields
 * @param x - left operand
 * @param y - rifht operand
 * @return x * y */
inline uint8_t mul(uint16_t x, uint16_t y){
    if (x == 0 || y == 0)
        return 0;
    return pgm_read_byte(exp + pgm_read_byte(log + x) + pgm_read_byte(log + y));
}

/* @brief Division in Galua Fields
 * @param x - dividend
 * @param y - divisor
 * @return x / y */
inline uint8_t div(uint8_t x, uint8_t y){
    assert(y != 0);
    if(x == 0) return 0;
    return pgm_read_byte(exp + (pgm_read_byte(log + x) + 255 - pgm_read_byte(log + y)) % 255);
}

/* @brief X in power Y w
 * @param x     - operand
 * @param power - power
 * @return x^power */
inline uint8_t pow(uint8_t x, intmax_t power){
    intmax_t i = pgm_read_byte(log + x);
    i *= power;
    i %= 255;
    if(i < 0) i = i + 255;
    return pgm_read_byte(exp + i);
}

/* @brief Inversion in Galua Fields
 * @param x - number
 * @return inversion of x */
inline uint8_t inverse(uint8_t x){
    return pgm_read_byte(exp + 255 - pgm_read_byte(log + x)); /* == div(1, x); */
}

/* ##########################
 * # POLYNOMIALS OPERATIONS #
 * ########################## */

/* @brief Multiplication polynomial by scalar
 * @param &p    - source polynomial
 * @param &newp - destination polynomial
 * @param x     - scalar */
inline void
poly_scale(const Poly *p, Poly *newp, uint16_t x) {
    newp->length = p->length;
    for(uint16_t i = 0; i < p->length; i++){
        newp->at(i) = mul(p->at(i), x);
    }
}

/* @brief Addition of two polynomials
 * @param &p    - right operand polynomial
 * @param &q    - left operand polynomial
 * @param &newp - destination polynomial */
inline void
poly_add(const Poly *p, const Poly *q, Poly *newp) {
    newp->length = poly_max(p->length, q->length);
    memset(newp->ptr(), 0, newp->length * sizeof(uint8_t));

    for(uint8_t i = 0; i < p->length; i++){
        newp->at(i + newp->length - p->length) = p->at(i);
    }

    for(uint8_t i = 0; i < q->length; i++){
        newp->at(i + newp->length - q->length) ^= q->at(i);
    }
}


/* @brief Multiplication of two polynomials
 * @param &p    - right operand polynomial
 * @param &q    - left operand polynomial
 * @param &newp - destination polynomial */
inline void
poly_mul(const Poly *p, const Poly *q, Poly *newp) {
    newp->length = p->length + q->length - 1;
    memset(newp->ptr(), 0, newp->length * sizeof(uint8_t));
    /* Compute the polynomial multiplication (just like the outer product of two vectors,
     * we multiply each coefficients of p with all coefficients of q) */
    for(uint8_t j = 0; j < q->length; j++){
        for(uint8_t i = 0; i < p->length; i++){
            newp->at(i+j) ^= mul(p->at(i), q->at(j)); /* == r[i + j] = gf_add(r[i+j], gf_mul(p[i], q[j])) */
        }
    }
}

/* @brief Division of two polynomials
 * @param &p    - right operand polynomial
 * @param &q    - left operand polynomial
 * @param &newp - destination polynomial */
inline void
poly_div(const Poly *p, const Poly *q, Poly *newp) {
    if(p->ptr() != newp->ptr()) {
        memcpy(newp->ptr(), p->ptr(), p->length*sizeof(uint8_t));
    }

    newp->length = p->length;

    uint8_t coef;

    for(int i = 0; i < (p->length-(q->length-1)); i++){
        coef = newp->at(i);
        if(coef != 0){
            for(uint8_t j = 1; j < q->length; j++){
                if(q->at(j) != 0)
                    newp->at(i+j) ^= mul(q->at(j), coef);
            }
        }
    }

    size_t sep = p->length-(q->length-1);
    memmove(newp->ptr(), newp->ptr()+sep, (newp->length-sep) * sizeof(uint8_t));
    newp->length = newp->length-sep;
}

/* @brief Evaluation of polynomial in x
 * @param &p - polynomial to evaluate
 * @param x  - evaluation point */
inline int8_t
poly_eval(const Poly *p, uint16_t x) {
    uint8_t y = p->at(0);
    for(uint8_t i = 1; i < p->length; i++){
        y = mul(y, x) ^ p->at(i);
    }
    return y;
}

} /* end of gf namespace */

#define MSG_CNT 3   // message-length polynomials count
#define POLY_CNT 14 // (ecc_length*2)-length polynomialc count

class ReedSolomon {
public:
    const uint8_t msg_length;
    const uint8_t ecc_length;

    uint8_t * generator_cache = nullptr;
    bool    generator_cached = false;

    ReedSolomon(uint8_t msg_length_p, uint8_t ecc_length_p) :
        msg_length(msg_length_p), ecc_length(ecc_length_p) {
            generator_cache = new uint8_t[ecc_length + 1];

            const uint8_t   enc_len  = msg_length + ecc_length;
            const uint8_t   poly_len = ecc_length * 2;
            uint8_t** memptr   = &memory;
            uint16_t  offset   = 0;

            /* Initialize first six polys manually cause their amount depends on template parameters */

            polynoms[0].Init(ID_MSG_IN, offset, enc_len, memptr);
            offset += enc_len;

            polynoms[1].Init(ID_MSG_OUT, offset, enc_len, memptr);
            offset += enc_len;

            for(uint8_t i = ID_GENERATOR; i < ID_MSG_E; i++) {
                polynoms[i].Init(i, offset, poly_len, memptr);
                offset += poly_len;
            }

            polynoms[5].Init(ID_MSG_E, offset, enc_len, memptr);
            offset += enc_len;

            for(uint8_t i = ID_TPOLY3; i < ID_ERR_EVAL+2; i++) {
                polynoms[i].Init(i, offset, poly_len, memptr);
                offset += poly_len;
            }
        }

    ~ReedSolomon() {
        delete [] generator_cache;
        // Dummy destructor, gcc-generated one crashes programm
        memory = NULL;
    }

    /* @brief Message block encoding
     * @param *src - input message buffer      (msg_lenth size)
     * @param *dst - output buffer for ecc     (ecc_length size at least) */
    void EncodeBlock(const void* src, void* dst) {
        assert(msg_length + ecc_length < 256);

        ///* Allocating memory on stack for polynomials storage */
        uint8_t stack_memory[MSG_CNT * msg_length + POLY_CNT * ecc_length * 2];
        this->memory = stack_memory;

        // gg : allocation is now on the heap
        //std::vector<uint8_t> stack_memory(MSG_CNT * msg_length + POLY_CNT * ecc_length * 2);
        //this->memory = stack_memory.data();

        const uint8_t* src_ptr = (const uint8_t*) src;
        uint8_t* dst_ptr = (uint8_t*) dst;

        Poly *msg_in  = &polynoms[ID_MSG_IN];
        Poly *msg_out = &polynoms[ID_MSG_OUT];
        Poly *gen     = &polynoms[ID_GENERATOR];

        // Weird shit, but without reseting msg_in it simply doesn't work
        msg_in->Reset();
        msg_out->Reset();

        // Using cached generator or generating new one
        if(generator_cached) {
            gen->Set(generator_cache, ecc_length + 1);
        } else {
            GeneratorPoly();
            memcpy(generator_cache, gen->ptr(), gen->length);
            generator_cached = true;
        }

        // Copying input message to internal polynomial
        msg_in->Set(src_ptr, msg_length);
        msg_out->Set(src_ptr, msg_length);
        msg_out->length = msg_in->length + ecc_length;

        // Here all the magic happens
        uint8_t coef = 0; // cache
        for(uint8_t i = 0; i < msg_length; i++){
            coef = msg_out->at(i);
            if(coef != 0){
                for(uint32_t j = 1; j < gen->length; j++){
                    msg_out->at(i+j) ^= gf::mul(gen->at(j), coef);
                }
            }
        }

        // Copying ECC to the output buffer
        memcpy(dst_ptr, msg_out->ptr()+msg_length, ecc_length * sizeof(uint8_t));
    }

    /* @brief Message encoding
     * @param *src - input message buffer      (msg_lenth size)
     * @param *dst - output buffer             (msg_length + ecc_length size at least) */
    void Encode(const void* src, void* dst) {
        uint8_t* dst_ptr = (uint8_t*) dst;

        // Copying message to the output buffer
        memcpy(dst_ptr, src, msg_length * sizeof(uint8_t));

        // Calling EncodeBlock to write ecc to out[ut buffer
        EncodeBlock(src, dst_ptr+msg_length);
    }

    /* @brief Message block decoding
     * @param *src         - encoded message buffer   (msg_length size)
     * @param *ecc         - ecc buffer               (ecc_length size)
     * @param *msg_out     - output buffer            (msg_length size at least)
     * @param *erase_pos   - known errors positions
     * @param erase_count  - count of known errors
     * @return RESULT_SUCCESS if successfull, error code otherwise */
    int DecodeBlock(const void* src, const void* ecc, void* dst, uint8_t* erase_pos = NULL, size_t erase_count = 0) {
        assert(msg_length + ecc_length < 256);

        const uint8_t *src_ptr = (const uint8_t*) src;
        const uint8_t *ecc_ptr = (const uint8_t*) ecc;
        uint8_t *dst_ptr = (uint8_t*) dst;

        const uint8_t src_len = msg_length + ecc_length;
        const uint8_t dst_len = msg_length;

        bool ok;

        ///* Allocation memory on stack  */
        uint8_t stack_memory[MSG_CNT * msg_length + POLY_CNT * ecc_length * 2];
        this->memory = stack_memory;

        // gg : allocation is now on the heap
        //std::vector<uint8_t> stack_memory(MSG_CNT * msg_length + POLY_CNT * ecc_length * 2);
        //this->memory = stack_memory.data();

        Poly *msg_in  = &polynoms[ID_MSG_IN];
        Poly *msg_out = &polynoms[ID_MSG_OUT];
        Poly *epos    = &polynoms[ID_ERASURES];

        // Copying message to polynomials memory
        msg_in->Set(src_ptr, msg_length);
        msg_in->Set(ecc_ptr, ecc_length, msg_length);
        msg_out->Copy(msg_in);

        // Copying known errors to polynomial
        if(erase_pos == NULL) {
            epos->length = 0;
        } else {
            epos->Set(erase_pos, erase_count);
            for(uint8_t i = 0; i < epos->length; i++){
                msg_in->at(epos->at(i)) = 0;
            }
        }

        // Too many errors
        if(epos->length > ecc_length) return 1;

        Poly *synd   = &polynoms[ID_SYNDROMES];
        Poly *eloc   = &polynoms[ID_ERRORS_LOC];
        Poly *reloc  = &polynoms[ID_TPOLY1];
        Poly *err    = &polynoms[ID_ERRORS];
        Poly *forney = &polynoms[ID_FORNEY];

        // Calculating syndrome
        CalcSyndromes(msg_in);

        // Checking for errors
        bool has_errors = false;
        for(uint8_t i = 0; i < synd->length; i++) {
            if(synd->at(i) != 0) {
                has_errors = true;
                break;
            }
        }

        // Going to exit if no errors
        if(!has_errors) goto return_corrected_msg;

        CalcForneySyndromes(synd, epos, src_len);
        FindErrorLocator(forney, NULL, epos->length);

        // Reversing syndrome
        // TODO optimize through special Poly flag
        reloc->length = eloc->length;
        for(int8_t i = eloc->length-1, j = 0; i >= 0; i--, j++){
            reloc->at(j) = eloc->at(i);
        }

        // Fing errors
        ok = FindErrors(reloc, src_len);
        if(!ok) return 1;

        // Error happened while finding errors (so helpfull :D)
        if(err->length == 0) return 1;

        /* Adding found errors with known */
        for(uint8_t i = 0; i < err->length; i++) {
            epos->Append(err->at(i));
        }

        // Correcting errors
        CorrectErrata(synd, epos, msg_in);

return_corrected_msg:
        // Wrighting corrected message to output buffer
        msg_out->length = dst_len;
        memcpy(dst_ptr, msg_out->ptr(), msg_out->length * sizeof(uint8_t));
        return 0;
    }

    /* @brief Message block decoding
     * @param *src         - encoded message buffer   (msg_length + ecc_length size)
     * @param *msg_out     - output buffer            (msg_length size at least)
     * @param *erase_pos   - known errors positions
     * @param erase_count  - count of known errors
     * @return RESULT_SUCCESS if successfull, error code otherwise */
    int Decode(const void* src, void* dst, uint8_t* erase_pos = NULL, size_t erase_count = 0) {
        const uint8_t *src_ptr = (const uint8_t*) src;
        const uint8_t *ecc_ptr = src_ptr + msg_length;

        return DecodeBlock(src, ecc_ptr, dst, erase_pos, erase_count);
    }

#ifndef DEBUG
private:
#endif

    enum POLY_ID {
        ID_MSG_IN = 0,
        ID_MSG_OUT,
        ID_GENERATOR,   // 3
        ID_TPOLY1,      // T for Temporary
        ID_TPOLY2,

        ID_MSG_E,       // 5

        ID_TPOLY3,     // 6
        ID_TPOLY4,

        ID_SYNDROMES,
        ID_FORNEY,

        ID_ERASURES_LOC,
        ID_ERRORS_LOC,

        ID_ERASURES,
        ID_ERRORS,

        ID_COEF_POS,
        ID_ERR_EVAL
    };

    // Pointer for polynomials memory on stack
    uint8_t* memory;
    Poly polynoms[MSG_CNT + POLY_CNT];

    void GeneratorPoly() {
        Poly *gen = polynoms + ID_GENERATOR;
        gen->at(0) = 1;
        gen->length = 1;

        Poly *mulp = polynoms + ID_TPOLY1;
        Poly *temp = polynoms + ID_TPOLY2;
        mulp->length = 2;

        for(int8_t i = 0; i < ecc_length; i++){
            mulp->at(0) = 1;
            mulp->at(1) = gf::pow(2, i);

            gf::poly_mul(gen, mulp, temp);

            gen->Copy(temp);
        }
    }

    void CalcSyndromes(const Poly *msg) {
        Poly *synd = &polynoms[ID_SYNDROMES];
        synd->length = ecc_length+1;
        synd->at(0) = 0;
        for(uint8_t i = 1; i < ecc_length+1; i++){
            synd->at(i) = gf::poly_eval(msg, gf::pow(2, i-1));
        }
    }

    void FindErrataLocator(const Poly *epos) {
        Poly *errata_loc = &polynoms[ID_ERASURES_LOC];
        Poly *mulp = &polynoms[ID_TPOLY1];
        Poly *addp = &polynoms[ID_TPOLY2];
        Poly *apol = &polynoms[ID_TPOLY3];
        Poly *temp = &polynoms[ID_TPOLY4];

        errata_loc->length = 1;
        errata_loc->at(0)  = 1;

        mulp->length = 1;
        addp->length = 2;

        for(uint8_t i = 0; i < epos->length; i++){
            mulp->at(0) = 1;
            addp->at(0) = gf::pow(2, epos->at(i));
            addp->at(1) = 0;

            gf::poly_add(mulp, addp, apol);
            gf::poly_mul(errata_loc, apol, temp);

            errata_loc->Copy(temp);
        }
    }

    void FindErrorEvaluator(const Poly *synd, const Poly *errata_loc, Poly *dst, uint8_t ecclen) {
        Poly *mulp = &polynoms[ID_TPOLY1];
        gf::poly_mul(synd, errata_loc, mulp);

        Poly *divisor = &polynoms[ID_TPOLY2];
        divisor->length = ecclen+2;

        divisor->Reset();
        divisor->at(0) = 1;

        gf::poly_div(mulp, divisor, dst);
    }

    void CorrectErrata(const Poly *synd, const Poly *err_pos, const Poly *msg_in) {
        Poly *c_pos     = &polynoms[ID_COEF_POS];
        Poly *corrected = &polynoms[ID_MSG_OUT];
        c_pos->length = err_pos->length;

        for(uint8_t i = 0; i < err_pos->length; i++)
            c_pos->at(i) = msg_in->length - 1 - err_pos->at(i);

        /* uses t_poly 1, 2, 3, 4 */
        FindErrataLocator(c_pos);
        Poly *errata_loc = &polynoms[ID_ERASURES_LOC];

        /* reversing syndromes */
        Poly *rsynd = &polynoms[ID_TPOLY3];
        rsynd->length = synd->length;

        for(int8_t i = synd->length-1, j = 0; i >= 0; i--, j++) {
            rsynd->at(j) = synd->at(i);
        }

        /* getting reversed error evaluator polynomial */
        Poly *re_eval = &polynoms[ID_TPOLY4];

        /* uses T_POLY 1, 2 */
        FindErrorEvaluator(rsynd, errata_loc, re_eval, errata_loc->length-1);

        /* reversing it back */
        Poly *e_eval = &polynoms[ID_ERR_EVAL];
        e_eval->length = re_eval->length;
        for(int8_t i = re_eval->length-1, j = 0; i >= 0; i--, j++) {
            e_eval->at(j) = re_eval->at(i);
        }

        Poly *X = &polynoms[ID_TPOLY1]; /* this will store errors positions */
        X->length = 0;

        int16_t l;
        for(uint8_t i = 0; i < c_pos->length; i++){
            l = 255 - c_pos->at(i);
            X->Append(gf::pow(2, -l));
        }

        /* Magnitude polynomial
           Shit just got real */
        Poly *E = &polynoms[ID_MSG_E];
        E->Reset();
        E->length = msg_in->length;

        uint8_t Xi_inv;

        Poly *err_loc_prime_temp = &polynoms[ID_TPOLY2];

        uint8_t err_loc_prime;
        uint8_t y;

        for(uint8_t i = 0; i < X->length; i++){
            Xi_inv = gf::inverse(X->at(i));

            err_loc_prime_temp->length = 0;
            for(uint8_t j = 0; j < X->length; j++){
                if(j != i){
                    err_loc_prime_temp->Append(gf::sub(1, gf::mul(Xi_inv, X->at(j))));
                }
            }

            err_loc_prime = 1;
            for(uint8_t j = 0; j < err_loc_prime_temp->length; j++){
                err_loc_prime = gf::mul(err_loc_prime, err_loc_prime_temp->at(j));
            }

            y = gf::poly_eval(re_eval, Xi_inv);
            y = gf::mul(gf::pow(X->at(i), 1), y);

            E->at(err_pos->at(i)) = gf::div(y, err_loc_prime);
        }

        gf::poly_add(msg_in, E, corrected);
    }

    bool FindErrorLocator(const Poly *synd, Poly *erase_loc = NULL, size_t erase_count = 0) {
        Poly *error_loc = &polynoms[ID_ERRORS_LOC];
        Poly *err_loc   = &polynoms[ID_TPOLY1];
        Poly *old_loc   = &polynoms[ID_TPOLY2];
        Poly *temp      = &polynoms[ID_TPOLY3];
        Poly *temp2     = &polynoms[ID_TPOLY4];

        if(erase_loc != NULL) {
            err_loc->Copy(erase_loc);
            old_loc->Copy(erase_loc);
        } else {
            err_loc->length = 1;
            old_loc->length = 1;
            err_loc->at(0)  = 1;
            old_loc->at(0)  = 1;
        }

        uint8_t synd_shift = 0;
        if(synd->length > ecc_length) {
            synd_shift = synd->length - ecc_length;
        }

        uint8_t K = 0;
        uint8_t delta = 0;
        uint8_t index;

        for(uint8_t i = 0; i < ecc_length - erase_count; i++){
            if(erase_loc != NULL)
                K = erase_count + i + synd_shift;
            else
                K = i + synd_shift;

            delta = synd->at(K);
            for(uint8_t j = 1; j < err_loc->length; j++) {
                index = err_loc->length - j - 1;
                delta ^= gf::mul(err_loc->at(index), synd->at(K-j));
            }

            old_loc->Append(0);

            if(delta != 0) {
                if(old_loc->length > err_loc->length) {
                    gf::poly_scale(old_loc, temp, delta);
                    gf::poly_scale(err_loc, old_loc, gf::inverse(delta));
                    err_loc->Copy(temp);
                }
                gf::poly_scale(old_loc, temp, delta);
                gf::poly_add(err_loc, temp, temp2);
                err_loc->Copy(temp2);
            }
        }

        uint32_t shift = 0;
        while(err_loc->length && err_loc->at(shift) == 0) shift++;

        uint32_t errs = err_loc->length - shift - 1;
        if(((errs - erase_count) * 2 + erase_count) > ecc_length){
            return false; /* Error count is greater then we can fix! */
        }

        memcpy(error_loc->ptr(), err_loc->ptr() + shift, (err_loc->length - shift) * sizeof(uint8_t));
        error_loc->length = (err_loc->length - shift);
        return true;
    }

    bool FindErrors(const Poly *error_loc, size_t msg_in_size) {
        Poly *err = &polynoms[ID_ERRORS];

        uint8_t errs = error_loc->length - 1;
        err->length = 0;

        for(uint8_t i = 0; i < msg_in_size; i++) {
            if(gf::poly_eval(error_loc, gf::pow(2, i)) == 0) {
                err->Append(msg_in_size - 1 - i);
            }
        }

        /* Sanity check:
         * the number of err/errata positions found
         * should be exactly the same as the length of the errata locator polynomial */
        if(err->length != errs)
            /* couldn't find error locations */
            return false;
        return true;
    }

    void CalcForneySyndromes(const Poly *synd, const Poly *erasures_pos, size_t msg_in_size) {
        Poly *erase_pos_reversed = &polynoms[ID_TPOLY1];
        Poly *forney_synd = &polynoms[ID_FORNEY];
        erase_pos_reversed->length = 0;

        for(uint8_t i = 0; i < erasures_pos->length; i++){
            erase_pos_reversed->Append(msg_in_size - 1 - erasures_pos->at(i));
        }

        forney_synd->Reset();
        forney_synd->Set(synd->ptr()+1, synd->length-1);

        uint8_t x;
        for(uint8_t i = 0; i < erasures_pos->length; i++) {
            x = gf::pow(2, erase_pos_reversed->at(i));
            for(int8_t j = 0; j < forney_synd->length - 1; j++){
                forney_synd->at(j) = gf::mul(forney_synd->at(j), x) ^ forney_synd->at(j+1);
            }
        }
    }
};

}

namespace GGWave {

// Direct-sequence spread magic numbers
// Used to xor the actual payload
const uint8_t kDSSMagic[] = {
    0x96, 0x9f, 0xb4, 0xaf, 0x1b, 0x91, 0xde, 0xc5, 0x45, 0x75, 0xe8, 0x2e, 0x0f, 0x32, 0x4a, 0x5f,
    0xb4, 0x56, 0x95, 0xcb, 0x7f, 0x6a, 0x54, 0x6a, 0x48, 0xf2, 0x0b, 0x7b, 0xcd, 0xfb, 0x93, 0x6d,
    0x3c, 0x77, 0x5e, 0xc3, 0x33, 0x47, 0xc0, 0xf1, 0x71, 0x32, 0x33, 0x27, 0x35, 0x68, 0x47, 0x1f,
    0x4e, 0xac, 0x23, 0x42, 0x5f, 0x00, 0x37, 0xa4, 0x50, 0x6d, 0x48, 0x24, 0x91, 0x7c, 0xa1, 0x4e,
};

uint8_t mymax(uint8_t a, uint8_t b) {
    return (a > b) ? a : b;
}

uint8_t getECCBytesForLength(uint8_t len) {
    return len < 4 ? 2 : mymax(4, 2*(len/5));
}

const uint8_t kDataLength_bytes = 16;
const uint8_t kECCLength_bytes = getECCBytesForLength(kDataLength_bytes);

typedef enum {
    GGWAVE_PROTOCOL_MT_NORMAL,
    GGWAVE_PROTOCOL_MT_FAST,
    GGWAVE_PROTOCOL_MT_FASTEST,
} TxProtocolId;

struct Parameters {
    int sampleRate;
    uint8_t freqStart;
    uint8_t timePerFrame_ms;
};

void send(uint8_t pin, const uint8_t * data, const Parameters & parameters) {
    const float dF = 48000.0/float(parameters.sampleRate);

    char buf[kDataLength_bytes];
    char enc[kDataLength_bytes + kECCLength_bytes];

    for (uint8_t i = 0; i < kDataLength_bytes; i++) {
        buf[i] = data[i] ^ kDSSMagic[i%sizeof(kDSSMagic)];
    }

    RS::ReedSolomon rsLength(kDataLength_bytes, kECCLength_bytes);
    rsLength.Encode(buf, enc);

    float fcur = 0.0f;
    for (int i = 0; i < 2*(kDataLength_bytes + kECCLength_bytes); i++) {
        uint8_t cur = enc[i/2];
        if (i%2 == 0) {
            cur = cur & 0x0F;
        } else {
            cur = cur >> 4;
        }

        fcur = float(parameters.freqStart)*dF + float(cur)*dF;

        tone(pin, fcur);
        delay(parameters.timePerFrame_ms);
    }

    noTone(pin);
    digitalWrite(pin, LOW);
}

void send(uint8_t pin, const uint8_t * data, TxProtocolId protocolId = GGWAVE_PROTOCOL_MT_FASTEST) {
    Parameters parameters = { 1024, 24, 64 };

    switch (protocolId) {
        case GGWAVE_PROTOCOL_MT_NORMAL:  parameters = { 1024, 24, 192 }; break;
        case GGWAVE_PROTOCOL_MT_FAST:    parameters = { 1024, 24, 128 }; break;
        case GGWAVE_PROTOCOL_MT_FASTEST: parameters = { 1024, 24,  64 }; break;
    };

    send(pin, data, parameters);
}

void send_text(uint8_t pin, const char * text, TxProtocolId protocolId = GGWAVE_PROTOCOL_MT_FASTEST) {
    char tx[kDataLength_bytes];
    memset(tx, 0, sizeof(tx));
    strncpy(tx, text, sizeof(tx));
    send(pin, (uint8_t *) tx, protocolId);
}

}
