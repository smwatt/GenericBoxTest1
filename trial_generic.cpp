///////////////////////////////////////////////////////////////////////////////
//
// trial_generic.cpp: Test tagging strategies using variations on a box concept.
//
///////////////////////////////////////////////////////////////////////////////
// (C) Copyright 2026 Stephen M. Watt
// Licensed under the 3-Clause BSD License (see LICENSE file).


#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <concepts>

///////////////////////////////////////////////////////////////////////////////
//
// Tagged Objects
//
//

typedef uint8_t  tag_t;

const static unsigned TAG_BITC = 3;
const static unsigned TAG_MASK = 0x7;

enum : tag_t {
    TAG_NIL  = 0,
    TAG_NAT  = 1,
    TAG_INT  = 2,
    TAG_FLO  = 3,
    TAG_CONS = 4,
    TAG_LIMIT  = 5
};

struct ObHead {
    tag_t       _tag;
    uint32_t    _wordc;

    ObHead(tag_t t, uint32_t c) : _tag(t), _wordc(c) {}
};
struct ObNil : public ObHead {
    ObNil() : ObHead(TAG_NIL, sizeof(ObNil)) { };
};
struct ObNat : public ObHead {
    uintptr_t   _val;
    ObNat(uintptr_t n) : ObHead(TAG_NAT, sizeof(ObNat)), _val(n) { };
};
struct ObInt : public ObHead {
    intptr_t   _val;
    ObInt(intptr_t  i) : ObHead(TAG_INT, sizeof(ObInt)), _val(i) { };
};
struct ObFlo : public ObHead {
    double      _val;
    ObFlo(double d) : ObHead(TAG_FLO, sizeof(ObFlo)), _val(d) { };
};
template <typename G>
struct ObCons : public ObHead {
    G   _car;
    G   _cdr;
    ObCons(G a, G d) : ObHead(TAG_CONS, sizeof(ObCons<G>)), _car(a), _cdr(d) { }
};

///////////////////////////////////////////////////////////////////////////////
//
// Box concept
//
template <class B>
concept Box = requires(B x, uintptr_t u, intptr_t i, double d) {
    { B::__Nil() } -> std::same_as<B>;
    { B::Nat(u) }  -> std::same_as<B>;
    { B::Int(i) }  -> std::same_as<B>;
    { B::Flo(d) }  -> std::same_as<B>;

    { x.tag() }     -> std::convertible_to<tag_t>;
    { x.wordc() }   -> std::convertible_to<uint32_t>;

    { x.qgetNat() } -> std::convertible_to<uintptr_t>;
    { x.qgetInt() } -> std::convertible_to<intptr_t>;
    { x.qgetFlo() } -> std::convertible_to<double>;
    { x.qCar() }    -> std::same_as<B>;
    { x.qCdr() }    -> std::same_as<B>;

    { x.release() } -> std::convertible_to<void>;
};

///////////////////////////////////////////////////////////////////////////////
//
// Heap allocated boxes.
//
struct HaBox {
private:
    void __() { static_assert(Box<HaBox>); }

    using _Cons = ObCons<HaBox>;

    struct ObHead *_p;

    HaBox(struct ObHead *p) : _p(p) { }

public:
    tag_t    tag()   const { return _p->_tag; }
    uint32_t wordc() const { return _p->_wordc; }

    static HaBox __Nil()            { return HaBox(new ObNil()); }
    static HaBox Nat(uintptr_t u)   { return HaBox(new ObNat(u)); }
    static HaBox Int(intptr_t  i)   { return HaBox(new ObInt(i)); }
    static HaBox Flo(double    d)   { return HaBox(new ObFlo(d)); }
    static HaBox Cons(HaBox a, HaBox d) { return HaBox(new _Cons(a, d)); }

    uintptr_t qgetNat() const { return ((ObNat *) _p)->_val; }
    intptr_t  qgetInt() const { return ((ObInt *) _p)->_val; }
    double    qgetFlo() const { return ((ObFlo *) _p)->_val; }
    HaBox     qCar()    const { return ((_Cons *) _p)->_car; }
    HaBox     qCdr()    const { return ((_Cons *) _p)->_cdr; }

    void release() { delete _p; }
};

const static HaBox HaBoxNil = HaBox::__Nil();

///////////////////////////////////////////////////////////////////////////////
//
// Lobit tagged boxes.
//
struct LoBox {
private:
    using _Cons = ObCons<LoBox>;

    void __() { static_assert(Box<LoBox>); }

    union U {
        struct ObHead *p;
        uintptr_t     u;
        intptr_t      i;
    } _v;

    static const uintptr_t PAYLOAD_MASK = ~(uintptr_t) TAG_MASK;
    LoBox(U v) : _v(v) { }

    template <typename T> T* pval() const {
        return reinterpret_cast<T *>(_v.u & ~(uintptr_t) TAG_MASK);
    }

public:
    tag_t    tag()   const { return _v.u & TAG_MASK; }

    uint32_t wordc() const {
        tag_t t = tag();
        return (t < TAG_FLO) ? 0 : pval<ObHead>()->_wordc;
    }

    static LoBox __Nil() {
        return LoBox(U{.u = TAG_NIL});
    }
    static LoBox Nat(uintptr_t u) {
        return LoBox(U{.u = (u << TAG_BITC) | TAG_NAT});
    }
    static LoBox Int(intptr_t i) {
        return LoBox(U{.i = (i << TAG_BITC) | TAG_INT});
    }
    static LoBox Flo(double d) {
        return LoBox(U{.u =  U{.p = new ObFlo(d)}.u | TAG_FLO});
    }
    static LoBox Cons(LoBox a, LoBox d) {
        return LoBox(U{.u =  U{.p = new _Cons(a, d)}.u | TAG_CONS});
    }

    uintptr_t qgetNat() const { return _v.u >> TAG_BITC; }
    intptr_t  qgetInt() const { return _v.i >> TAG_BITC; }
    double    qgetFlo() const { return pval<ObFlo>()->_val; }
    LoBox     qCar()    const { return pval<_Cons>()->_car; }
    LoBox     qCdr()    const { return pval<_Cons>()->_cdr; }

    void release() { if (tag() >= TAG_FLO) delete pval<ObHead>(); }
};
const static LoBox LoBoxNil = LoBox::__Nil();

///////////////////////////////////////////////////////////////////////////////
//
// NanBoxes.
// 

struct NanBox {
private:
    using _Cons = ObCons<NanBox>;

    void __() { static_assert(Box<NanBox>); }

    union U {
        double   d;
        uint64_t u;
    } _v;

    static constexpr uint64_t QNAN_MASK  = 0x7ff8000000000000ull;
    static constexpr uint64_t TAG_MASK   = 0x0007000000000000ull;
    static constexpr uint64_t VAL_MASK   = 0x0000ffffffffffffull;
    static constexpr uint64_t TOP_MASK   = 0xffff000000000000ull;
    static constexpr uint64_t TAG_UNIT   = 0x0001000000000000ull;

    static constexpr unsigned VAL_BITC = 48;
    static constexpr unsigned TOP_BITC = 16;

    static constexpr uint64_t CANON_QNAN = 0x7ff8000000000000ull;
    static constexpr uint64_t NIL_VAL    = 0x1ull;


    explicit NanBox(U v) : _v(v) { }

    explicit NanBox(tag_t t, uint64_t uval)
        : _v{.u = top_tag(t) | (uval & VAL_MASK)} { }

    explicit NanBox(tag_t t, ObHead *pval)
        : NanBox(t, ptr_bits(pval)) { }


    static constexpr uint64_t top_tag(tag_t t) {
        return QNAN_MASK | t * TAG_UNIT;
    }
    static constexpr bool has_qnan_bits(uint64_t u) {
        return (u & QNAN_MASK) == QNAN_MASK;
    }
    static constexpr bool is_float_bits(uint64_t u) {
        return ((u & QNAN_MASK)!= QNAN_MASK) | (u== CANON_QNAN);
    }
    static constexpr int64_t sign_extend_48(uint64_t u) {
        return int64_t(u << TOP_BITC) >> TOP_BITC;
    }
    static constexpr bool fits_int48(intptr_t i) {
        return sign_extend_48(uint64_t(i)) == i;
    }
    static uintptr_t ptr_bits(ObHead *pval) {
        uintptr_t p = reinterpret_cast<uintptr_t>(pval);
        assert((p & TOP_MASK)== 0 || (p & TOP_MASK)== TOP_MASK);
        return p;
    }


    int64_t  ival() const { return sign_extend_48(_v.u); }
    uint64_t uval() const { return _v.u & VAL_MASK; }

    template <typename T>
    T *pval() const {
        return reinterpret_cast<T *>(uintptr_t(ival()));
    }

public:
    tag_t tag() const {
        if (is_float_bits(_v.u)) return TAG_FLO;
        return tag_t((_v.u & TAG_MASK) >> VAL_BITC);
    }
    uint32_t wordc() const {
        return tag() <= TAG_FLO ? 0 : pval<ObHead>()->_wordc;
    }

    static NanBox __Nil() {
        return NanBox(TAG_NIL, NIL_VAL);
    }
    static NanBox Nat(uintptr_t u) {
        assert((u & TOP_MASK) == 0);
        return NanBox(TAG_NAT, u);
    }
    static NanBox Int(intptr_t i) {
        assert(fits_int48(i));
        return NanBox(TAG_INT, uint64_t(i) & VAL_MASK);
    }
    static NanBox Flo(double d) {
        U v{.d = d};
        if (has_qnan_bits(v.u)) v.u = CANON_QNAN;
        return NanBox(v);
    }
    static NanBox Cons(NanBox a, NanBox d) {
        return NanBox(TAG_CONS, new _Cons(a, d));
    }

    intptr_t  qgetInt() const { return ival(); }
    uintptr_t qgetNat() const { return uval(); }
    double    qgetFlo() const { return _v.d; }
    NanBox    qCar()    const { return pval<_Cons>()->_car; }
    NanBox    qCdr()    const { return pval<_Cons>()->_cdr; }

    void release() { if (tag() > TAG_FLO) delete pval<_Cons>(); }
};
inline const NanBox NanBoxNil = NanBox::__Nil();


/******************************************************************************
 *
 * Generic conversion and timing tests for any Box implementation.
 *
 */

#include <new>
#include <string.h>

/******************************************************************************
 *
 * Conversion Tests
 *
 */

template <Box B>
void test_conversions(const char *name) {
    printf("[Test] %s conversions\n", name);

    B nil = B::__Nil();
    assert(nil.tag() == TAG_NIL);
    B nat = B::Nat(1234567);
    assert(nat.tag() == TAG_NAT);
    assert(nat.qgetNat() == 1234567);

    B integer = B::Int(7654321);
    assert(integer.tag() == TAG_INT);
    assert(integer.qgetInt() == 7654321);

    double d = 3.14159265358979323846;
    B flo = B::Flo(d);
    assert(flo.tag() == TAG_FLO);
    assert(flo.qgetFlo() == d);

    B cons = B::Cons(B::Nat(11), B::Int(22));
    assert(cons.tag() == TAG_CONS);
    assert(cons.qCar().tag() == TAG_NAT);
    assert(cons.qCar().qgetNat() == 11);
    assert(cons.qCdr().tag() == TAG_INT);
    assert(cons.qCdr().qgetInt() == 22);

    printf("  - Success.\n");
}

void test_all_conversions() {
    printf("--- STARTING ALL CONVERSION TESTS ---\n\n");
    test_conversions<HaBox> ("HaBox");
    test_conversions<LoBox> ("LoBox");
    test_conversions<NanBox>("NanBox");
    printf("\n--- ALL TESTS PASSED ---\n");
}

//////////////////////////////////////////////////////////////////////////////
//
// Make an array of random boxed values, with equal chance to be nil, nat,
// double or cons.  The values start at 0 and increment by 1.  After creating
// the array, it is shuffled so traversing the array in order addresses memory
// randomly.

// Stable seed for reproducible benchmarks.
// See "Fast Splittable Pseudorandom Number Generators",
// Guy L. Steele Jr., Doug Lea, and Christine H. Flood, OOPSLA '14

static constexpr uint64_t bench_seed = 0x123456789ABCDEF0ULL;
static uint64_t bench_state = bench_seed;

void reset_bench_state() {
    bench_state = bench_seed;
}

size_t randmod(size_t m) {
    if (m == 0) return 0;

    /* SplitMix64 algorithm */
    uint64_t z = (bench_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);

    /* Return result in range [0, m-1]. */
    return size_t(z % m);
}

template <Box B>
void shuffle(size_t argc, B *argv) {
    for (size_t i = argc-1; i >= 1; i--) {
        size_t j = randmod(i+1);
        B t = argv[i];
        argv[i] = argv[j];
        argv[j] = t;
    }
}

template <Box B>
B *mustAllocBoxArray(size_t argc) {
    B *p = static_cast<B *>(::operator new[](argc * sizeof(B)));
    if (!p) {
        fprintf(stderr, "Out of memory allocating %zu boxes.\n", argc);
        exit(EXIT_FAILURE);
    }
    return p;
}

template <Box B>
B *makeRandomBoxArray(size_t argc) {
    uint32_t modulus = 1000;
    uint64_t counter = 0;
    B *obv = mustAllocBoxArray<B>(argc);

   /* Fill it with 1/4 nil, 1/4 nat, 1/4 double, 1/4 cons. */
    for (size_t i = 0; i < argc; i++) {
        switch (i % 5) {
        case 0:
            new (&obv[i]) B(B::__Nil());
            break;
        case 1:
            new (&obv[i]) B(B::Nat(counter++ % modulus));
            break;
        case 2:
            new (&obv[i]) B(B::Int((counter++ % modulus) - modulus/2));
            break;
        case 3:
            new (&obv[i]) B(B::Flo(double(counter++ % modulus)));
            break;
        case 4: {
            B a = B::Nat(counter++ % modulus);
            B d = B::Nat(counter++ % modulus);
            new (&obv[i]) B(B::Cons(a, d));
            break;
          }
        }
    }

    shuffle(argc, obv);
    return obv;
}

template <Box B>
void releaseBoxArray(size_t obc, B *obv) {
    for (size_t i = 0; i < obc; i++) {
        B ob = obv[i];;
        if (ob.tag() == TAG_CONS) { 
            ob.qCar().release();
            ob.qCdr().release();
        }
        ob.release();
    }
    ::operator delete[](obv);
}

//////////////////////////////////////////////////////////////////////////////
//
// Timing
//
static double
now_seconds(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

//////////////////////////////////////////////////////////////////////////////
//
// Time tag count
//
template <Box B>
double time_tag_count(const char *name, size_t nelts) {
    B *obv = makeRandomBoxArray<B>(nelts);

    size_t counts[TAG_LIMIT];
    double start, duration;
    int    loopGroup = 25, quo = nelts / loopGroup, rem = nelts % loopGroup;
    int    ix;

    printf("%s tags/1000 ", name);
    for (int i = 0; i < TAG_LIMIT; i++) counts[i] = 0;

    start = now_seconds();
    ix = 0;
    for (int i = 0; i < rem; i++) counts[obv[ix++].tag()]++;
    for (int i = 0; i < quo; i++) {
        // Need loopGroup number of lines here.
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;

        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;

        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;

        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;

        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
        counts[obv[ix++].tag()]++;
    }
    duration = now_seconds() - start;

    printf("nil=%ld nat=%ld int=%ld flo=%ld cons=%ld ",
        (long) counts[TAG_NIL]/1000,  (long) counts[TAG_NAT]/1000,
        (long) counts[TAG_INT]/1000,  (long) counts[TAG_FLO]/1000,
        (long) counts[TAG_CONS]/1000);

    printf("in %.6fms", 1000*duration);
    printf("\n");

    releaseBoxArray(nelts, obv);
    return duration;
}

//////////////////////////////////////////////////////////////////////////////
//
// Time unboxing arithmetic
//

static const long sum_modulus = 99991;

template <Box B>
double time_additions(const char *name, size_t nelts) {
    B *obv = makeRandomBoxArray<B>(nelts);

    long long unsigned  nat_sum = 0;
    long long int       int_sum = 0;
    double              flo_sum = 0.0;

    double start, duration;

    printf("%s sums: ", name);

    start = now_seconds();
    for (int i = 0; i < nelts; i++) {
        B ob = obv[i];
        switch(ob.tag()) {
        case TAG_NAT:  nat_sum += ob.qgetNat(); break;
        case TAG_INT:  int_sum += ob.qgetInt(); break;
        case TAG_CONS: nat_sum += ob.qCar().qgetNat() + ob.qCdr().qgetNat();
        }
        if (i % 100000 == 0) {
            nat_sum %= sum_modulus;
            int_sum %= sum_modulus;
        }
    }
    nat_sum %= sum_modulus;
    int_sum %= sum_modulus;

    duration = now_seconds() - start;

    printf("nat=%lld int=%lld flo=%g ", nat_sum, int_sum, flo_sum);

    printf("in %.6fms", 1000*duration);
    printf("\n");

    releaseBoxArray(nelts, obv);
    return duration;
}


//////////////////////////////////////////////////////////////////////////////
//
// Time boxed arithmetic
//


template <Box B>
double time_boxmath(const char *name, size_t nelts) {
    B *obv = makeRandomBoxArray<B>(nelts);

    B nat_sum = B::Nat(0);
    B int_sum = B::Int(0);
    B flo_sum = B::Flo(0.0);

    double start, duration;

    printf("%s bmath: ", name);

    start = now_seconds();
    for (int i = 0; i < nelts; i++) {
        B ob = obv[i];
        switch(ob.tag()) {
        case TAG_NAT: {
            B tn = B::Nat((nat_sum.qgetNat() + ob.qgetNat()));
            nat_sum.release();
            nat_sum = tn;
            break;
          }
        case TAG_INT: {
            B ti = B::Int((int_sum.qgetInt() + ob.qgetInt()));
            int_sum.release();
            int_sum = ti;
            break;
          }
        case TAG_FLO: {
            break;
            //B tf = B::Flo((flo_sum.qgetFlo() + ob.qgetFlo()));
            //flo_sum.release();
            //flo_sum = tf;
            //break;
          }
        case TAG_CONS: {
            B tc = B::Nat((nat_sum.qgetNat() +
                          ob.qCar().qgetNat() + ob.qCdr().qgetNat()));
            nat_sum.release();
            nat_sum = tc;
            break;
          }
        }
        if (i % 100000 == 0) {
            B ob = B::Nat(nat_sum.qgetNat() % sum_modulus);
            nat_sum.release();
            nat_sum = ob;

            ob = B::Int(int_sum.qgetInt() % sum_modulus);
            int_sum.release();
            int_sum = ob;
        }
    }
    B ob = B::Nat(nat_sum.qgetNat() % sum_modulus);
    nat_sum.release();
    nat_sum = ob;

    ob = B::Int(int_sum.qgetInt() % sum_modulus);
    int_sum.release();
    int_sum = ob;

    duration = now_seconds() - start;

    printf("nat=%ld int=%ld flo=%g ", (long) nat_sum.qgetNat(), (long) int_sum.qgetInt(), flo_sum.qgetFlo());

    printf("in %.6fms", 1000*duration);
    printf("\n");

    nat_sum.release();
    int_sum.release();
    flo_sum.release();

    releaseBoxArray(nelts, obv);
    return duration;
}

//////////////////////////////////////////////////////////////////////////////
//
// Main
//
// Call with argument --testconv to test conversions.
// Call with argument --sizes to see sizes of objects.
// Call with argument --time N to run timing tests.
//

int main(int argc, char **argv) {
    
    if (argc == 1)
        printf("Usage: trial [--testconv | --sizes | --time n]\n");

    if (argc > 1 && !strcmp(argv[1], "--testconv"))
        test_all_conversions();

    if (argc > 1 && !strcmp(argv[1], "--sizes")) {
        printf("sizeof ObHead:  %d\n", (int) sizeof(ObHead));
        printf("sizeof ObNil:   %d\n", (int) sizeof(ObNil));
        printf("sizeof ObNat:   %d\n", (int) sizeof(ObNat));
        printf("sizeof ObFlo:   %d\n", (int) sizeof(ObFlo));

        printf("sizeof HaBox:   %d\n", (int) sizeof(HaBox));
        printf("sizeof HaCons:  %d\n", (int) sizeof(ObCons<HaBox>));

        printf("sizeof LoBox:   %d\n", (int) sizeof(LoBox));
        printf("sizeof LoCons:  %d\n", (int) sizeof(ObCons<LoBox>));

        printf("sizeof NanBox:  %d\n", (int) sizeof(NanBox));
        printf("sizeof NanCons: %d\n", (int) sizeof(ObCons<NanBox>));
        
    }

    if (argc > 2 && !strcmp(argv[1], "--time")) {
        size_t nelts = atol(argv[2]);

        printf("Time %ld =============================\n", (long) nelts);
        double haduration, loduration, nanduration;

        haduration  = time_tag_count<HaBox> ("HaBox ", nelts);
        loduration  = time_tag_count<LoBox> ("LoBox ", nelts);
        nanduration = time_tag_count<NanBox>("NanBox", nelts);
        printf("Heap/Lo  = %g\n", haduration/loduration);
        printf("Heap/Nan = %g\n", haduration/nanduration);

        haduration  = time_additions<HaBox> ("HaBox ", nelts);
        loduration  = time_additions<LoBox> ("LoBox ", nelts);
        nanduration = time_additions<NanBox>("NanBox", nelts);
        printf("Heap/Lo  = %g\n", haduration/loduration);
        printf("Heap/Nan = %g\n", haduration/nanduration);

        haduration  = time_boxmath<HaBox> ("HaBox ", nelts);
        loduration  = time_boxmath<LoBox> ("LoBox ", nelts);
        nanduration = time_boxmath<NanBox>("NanBox", nelts);
        printf("Heap/Lo  = %g\n", haduration/loduration);
        printf("Heap/Nan = %g\n", haduration/nanduration);
    }

    return 0;
}

