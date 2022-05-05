/* Author: Mike Lubinets (aka mersinvald)
 * Date: 29.12.15
 *
 * See LICENSE */

#ifndef RS_HPP
#define RS_HPP

#include "poly.hpp"
#include "gf.hpp"

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <vector>

namespace RS {

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
        //uint8_t stack_memory[MSG_CNT * msg_length + POLY_CNT * ecc_length * 2];
        //this->memory = stack_memory;

        // gg : allocation is now on the heap
        std::vector<uint8_t> stack_memory(MSG_CNT * msg_length + POLY_CNT * ecc_length * 2);
        this->memory = stack_memory.data();

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
        //uint8_t stack_memory[MSG_CNT * msg_length + POLY_CNT * ecc_length * 2];
        //this->memory = stack_memory;

        // gg : allocation is now on the heap
        std::vector<uint8_t> stack_memory(MSG_CNT * msg_length + POLY_CNT * ecc_length * 2);
        this->memory = stack_memory.data();

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

#endif // RS_HPP

