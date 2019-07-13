/**
 * This file is part of Rellume.
 *
 * (c) 2016-2019, Alexis Engelke <alexis.engelke@googlemail.com>
 *
 * Rellume is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Rellume is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Rellume.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 **/

#ifndef LL_REGFILE_H
#define LL_REGFILE_H

#include "llcommon-internal.h"
#include "rellume/instr.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>
#include <cstdbool>
#include <cstdint>
#include <algorithm>


/**
 * \ingroup LLRegFile
 **/
enum {
    /**
     * \brief The zero flag
     **/
    RFLAG_ZF = 0,
    /**
     * \brief The sign flag
     **/
    RFLAG_SF,
    /**
     * \brief The parity flag
     **/
    RFLAG_PF,
    /**
     * \brief The carry flag
     **/
    RFLAG_CF,
    /**
     * \brief The overflow flag
     **/
    RFLAG_OF,
    /**
     * \brief The auxiliary carry flag
     **/
    RFLAG_AF,
    RFLAG_Max
};

class Facet {
public:
    enum Value {
        I64,
        I32, I16, I8, I8H, PTR,

        I128,
        V1I8, V2I8, V4I8, V8I8, V16I8,
        V1I16, V2I16, V4I16, V8I16,
        V1I32, V2I32, V4I32,
        V1I64, V2I64,
        V1F32, V2F32, V4F32,
        V1F64, V2F64,
        F32, F64,
#if LL_VECTOR_REGISTER_SIZE >= 256
        I256,
#endif

        // Pseudo-facets
        I, VI8, VI16, VI32, VI64, VF32, VF64,
        MAX,

#if LL_VECTOR_REGISTER_SIZE == 128
        IVEC = I128,
#elif LL_VECTOR_REGISTER_SIZE == 256
        IVEC = I256,
#endif
    };

    llvm::Type* Type(llvm::LLVMContext& ctx);
    Facet Resolve(size_t bits);

    Facet() = default;
    constexpr Facet(Value value) : value(value) {}
    operator Value() const { return value; }
    explicit operator bool() = delete;
private:
    Value value;
};

template<Facet::Value... E>
class ValueMap {
    template<typename T, int N, int M>
    struct LookupTable {
        constexpr LookupTable(std::initializer_list<T> il) : f(), b() {
            int i = 0;
            for (auto elem : il) {
                f[i] = elem;
                b[static_cast<int>(elem)] = 1 + i++;
            }
        }
        T f[N];
        unsigned b[M];
    };

    static const LookupTable<Facet::Value, sizeof...(E), Facet::MAX> table;
    llvm::Value* values[sizeof...(E)];
public:
    llvm::Value*& at(Facet v) {
        assert(table.b[static_cast<int>(v)] > 0);
        return values[table.b[static_cast<int>(v)] - 1];
    }
    // This returns a reference to an array of size sizeof...(E).
    const Facet::Value (&facets())[sizeof...(E)] {
        return table.f;
    }
    void clear() {
        std::fill_n(values, sizeof...(E), nullptr);
    }
};
template<Facet::Value... E>
const typename ValueMap<E...>::template LookupTable<Facet::Value, sizeof...(E), Facet::MAX> ValueMap<E...>::table({E...});

typedef ValueMap<Facet::I64, Facet::I32, Facet::I16, Facet::I8, Facet::I8H, Facet::PTR> ValueMapGp;
typedef ValueMap<Facet::I128,
#if LL_VECTOR_REGISTER_SIZE >= 256
    Facet::I256,
#endif
    Facet::I8, Facet::V1I8, Facet::V2I8, Facet::V4I8, Facet::V8I8, Facet::V16I8,
    Facet::I16, Facet::V1I16, Facet::V2I16, Facet::V4I16, Facet::V8I16,
    Facet::I32, Facet::V1I32, Facet::V2I32, Facet::V4I32,
    Facet::I64, Facet::V1I64, Facet::V2I64,
    Facet::F32, Facet::V1F32, Facet::V2F32, Facet::V4F32,
    Facet::F64, Facet::V1F64, Facet::V2F64
> ValueMapSse;

class RegFile
{
public:
    struct FlagCache {
        bool valid;
        llvm::Value* lhs;
        llvm::Value* rhs;

        FlagCache() : valid(false) {}
        void update(llvm::Value* op1, llvm::Value* op2) {
            lhs = op1; rhs = op2; valid = true;
        }
    };

    RegFile(llvm::BasicBlock* llvm_block) : llvm_block(llvm_block),
            flag_cache() {}

    llvm::Value* GetReg(LLReg reg, Facet facet);
    void SetReg(LLReg reg, Facet facet, llvm::Value*, bool clear_facets);
    void Rename(LLReg reg_dst, LLReg reg_src);

    llvm::Value* GetFlag(int flag);
    void SetFlag(int flag, llvm::Value*);

    RegFile::FlagCache& GetFlagCache() {
        return flag_cache;
    }

private:
    llvm::BasicBlock* llvm_block;
    ValueMapGp regs_gp[LL_RI_GPMax];
    ValueMapSse regs_sse[LL_RI_XMMMax];
    llvm::Value* reg_ip;
    llvm::Value* flags[RFLAG_Max];

    FlagCache flag_cache;
};

#endif
