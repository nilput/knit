#define KNIT_OBJ_HEAD \
    int ktype
struct knit_str {
    KNIT_OBJ_HEAD;
    char *str; //null terminated
    int len;
    int cap; //negative values mean memory is not owned by us (const char * passed to us)
};

struct knit_obj; //fwd

struct knit_list {
    KNIT_OBJ_HEAD;
    struct knit_obj **items;
    int len;
    int cap;
};
struct knit_int {
    KNIT_OBJ_HEAD;
    int value;
};
struct knit; //fwd

#include "knit_objp_darr.h" //autogenerated darr.h and prefixed by knit_objp_
//TODO: check these speical values aren't exceeded during normal operation
#define KINSN_ADDR_UNK 16000 //a special value used during backpatching
#define KRES_UNKNOWN_KEEP_RET 128        //a special value for expecting an unknown number of returns at function calls
#define KRES_UNKNOWN_DISCARD_RET 129     //a special value for discarding an unknown number of returns at function calls
struct knit_insn {
    int insn_type;
    int op1;
    int op2;
};
#include "insns_darr.h"
struct knit_block { 
    //this can't contain self references, there is code that assumes it is memcopyable
    int nlocals;
    struct insns_darr insns;
    struct knit_objp_darr constants;
};

typedef int (*knit_func_type)(struct knit *);
struct knit_cfunc {
    int ktype;
    knit_func_type fptr;
};
struct knit_kfunc {
    int ktype;
    struct knit_block block;
};

//this is used to store true, false, and null
struct knit_bvalue {
    KNIT_OBJ_HEAD;
};

struct knit_obj {
    union knit_obj_u {
        int ktype;
        struct knit_list list;
        struct knit_str str;
        struct knit_int integer;
        struct knit_cfunc cfunc;
        struct knit_kfunc kfunc; //huge?
        struct knit_bvalue bval; 
    } u;
};

enum KNIT_RV {
    KNIT_OK = 0,
    KNIT_NOMEM,
    KNIT_INVALID_TYPE_ERR,
    KNIT_SYNTAX_ERR,
    KNIT_RUNTIME_ERR,
    KNIT_DEINIT_ERR,
    KNIT_OUT_OF_RANGE_ERR,
    KNIT_NOT_FOUND = -1,
    KNIT_DUPLICATE_KEY = -1,
    //informational internal rvs (not errors)
    KNIT_CREATED_NEW = -2,
    KNIT_RETRIEVED   = -3,
};
enum KNIT_TYPE {
    KNIT_NULL = 0xd6,
    KNIT_STR,
    KNIT_LIST,
    KNIT_INT,
    KNIT_CFUNC,
    KNIT_KFUNC,
    KNIT_TRUE,
    KNIT_FALSE,
};
enum KNIT_OPT {
    KNIT_POLICY_EXIT = 1, //default
    KNIT_POLICY_CONTINUE = 2,
};

#include "hasht/third_party/strhash/superfasthash.h"

/*hashtable defs*/
    typedef struct knit_str    vars_hasht_key_type;   //internal notes: there is no indirection, init/deinit must be used
    typedef struct knit_obj  * vars_hasht_value_type; //                there is indirection,    new/destroy must be used
    int vars_hasht_key_eq_cmp(vars_hasht_key_type *key_1, vars_hasht_key_type *key_2) {
        struct knit_str *str1 = key_1;
        struct knit_str *str2 = key_2;
        if (str1->len != str2->len)
            return 1;
        return memcmp(str1->str, str2->str, str1->len); //0 if eq
    }
    size_t vars_hasht_hash(vars_hasht_key_type *key) {
        struct knit_str *str = key;
        return SuperFastHash(str->str, str->len);
    }

    typedef void * mem_hasht_key_type; 
    typedef unsigned char mem_hasht_value_type; //we don't really need a value, it is a set
    int mem_hasht_key_eq_cmp(mem_hasht_key_type *key_1, mem_hasht_key_type *key_2) {
        //we are comparing two void pointers
        if (*key_1 == *key_2)
            return 0; 
        return 1;
    }
    size_t mem_hasht_hash(mem_hasht_key_type *key) {
        return (uintptr_t) *key; //casting a void ptr to an integer
    }
/*end of hashtable defs*/

#include "vars_hasht.h" //autogenerated hasht.h and prefixed by vars_
#include "mem_hasht.h"  //autogenerated hasht.h and prefixed by mem_

enum KNIT_FRAME_TYPE {
    KNIT_FRAME_KBLOCK, //execution of vm insn blocks
    KNIT_FRAME_CFUNC, //a C call
};
struct knit_frame {
        /*
        * stack:
        *
        *      arg   0
        *      arg   1
        *      ...
        *bsp:  local 0
        *      local 1
        *      local 2
        *      temp  1
        *      ...
        */
    short frame_type;
    short nargs;
    short nexpected_returns;
    int bsp; //base stack pointer, (where locals indice are based on) this refers to the global values stack
    union {
        struct {
            struct knit_block *block;
            int ip;
        } kf;
        struct {
            struct knit_cfunc *cfunc;
        } cf;
    } u;
};
#include "knit_frame_darr.h"
struct knit_stack {
    struct knit_frame_darr frames; //contains information about function calls and IPs
    struct knit_objp_darr vals;    //contains the objects (ints, strs, lists ...) pushed on stack
};

struct knit_exec_state {
    struct vars_hasht global_ht;
    struct knit_stack stack;
    int nresults; //the number of results returned by the last executed KRET statement
    int last_cond;
};

struct knit_tok {
    int toktype;
    int lineno;
    int colno;
    int offset;
    int len;
    union {
        int integer;
    } data;
};
#include "tok_darr.h"      //autogenerated darr.h and prefixed by tok_

struct knit {
    struct mem_hasht mem_ht;

    struct knit_exec_state ex;

    char *err_msg;
    unsigned char is_err_msg_owned;
    int err;
    int err_policy;
};

enum KATOK {
    KAT_EOF,
    KAT_BOF, //begining of file

    KAT_FUNCTION, //'function' keyword
    KAT_RETURN, //'return' keyword
    KAT_NULL,  //'null'  keyword
    KAT_TRUE,  //'true'  keyword
    KAT_FALSE, //'false' keyword

    KAT_LAND, //'and' keyword
    KAT_LOR,  //'or'  keyword

    KAT_STRLITERAL,
    KAT_INTLITERAL,
    KAT_VAR,
    KAT_DOT,
    KAT_ASSIGN,
    KAT_OPAREN,
    KAT_CPAREN,
    KAT_OBRACKET, // '['
    KAT_CBRACKET, // ']'
    KAT_OCURLY,   // '{'
    KAT_CCURLY,   // '}'

    KAT_OP_NOT,    // '!'
    KAT_OP_EQ,     // '=='
    KAT_OP_NEQ,    // '!='
    KAT_OP_GT,     // '>'
    KAT_OP_LT,     // '<'
    KAT_OP_GTEQ,   // '>='
    KAT_OP_LTEQ,   // '<='

    KAT_COMMA,
    KAT_COLON,
    KAT_SEMICOLON,
    KAT_ADD,
    KAT_SUB,
    KAT_MUL,
    KAT_DIV,
    KAT_MOD,

    //not tokens
    KAT_ASSOC_LEFT,
    KAT_ASSOC_RIGHT,
};
enum KAEXPR {
    KAX_CALL,
    KAX_ASSIGNMENT,
    KAX_LITERAL_STR,
    KAX_LITERAL_INT,
    KAX_LITERAL_LIST,
    KAX_LITERAL_TRUE,
    KAX_LITERAL_FALSE,
    KAX_LITERAL_NULL,
    KAX_FUNCTION,
    KAX_LIST_INDEX,
    KAX_LIST_SLICE,
    KAX_VAR_REF,
    KAX_OBJ_DOT,
    KAX_BIN_OP,
    KAX_LOGICAL_BINOP,
    KAX_UN_OP,
};

struct knit_exp; //fwd
#include "knit_expr_darr.h"

struct knit_call {
    struct knit_expr *called;
    struct knit_expr_darr args;
};
struct knit_varname_chain {
    struct knit_str *name;
    struct knit_varname_chain *next; //a.b.c
};
struct knit_patch_list {
    int instruction_address;
    struct knit_patch_list *next;
};
struct knit_expr {
    int exptype;
    union {

        struct knit_str *str;

        struct knit_list *list;

        int integer;

        struct knit_expr *exp;

        struct { 
            struct knit_expr *exp;
            struct knit_expr *beg;
            struct knit_expr *end;
        } slice;

        struct { 
            int op; //an INSN (KADD, KSUB...)
            struct knit_expr *lhs;
            struct knit_expr *rhs;
        } bin;
        struct { 
            int op; //a logical binary op, either AND or OR to be short circuited by branching
            struct knit_expr *lhs;
            struct knit_expr *rhs;
            struct knit_patch_list *plist;
        } logic_bin;
        struct { 
            int op; //an INSN (KNEG, etc..)
            struct knit_expr *operand;
        } un;
        struct {
            int varname_idx; //a reference to curblk->locals[]
            struct knit_str *name;
        } varref;

        struct {
            struct knit_expr *parent;
            struct knit_varname_chain *chain;
        } prefix;
            /*
               example 1:
               a . b . c
               ^^^e1^^^^^
                   ^^e2^^
                e1 : {
                    exptype: KAX_OBJ_DOT
                    lhs : { 
                        exptype: KAX_VAR_REF
                        str: "a"
                    }
                    rhs (e2): {
                       exptype: KAX_OBJ_DOT,
                       lhs: {
                            exptype: KAX_VAR_REF
                            str: "b"
                       }
                       rhs: {
                            exptype: KAX_VAR_REF
                            str: "c"
                       }
                    }
                }
                       
               example 2:
               a . b . c  [ 1000 ]
               ^^OBJDOT^  ^^INDEX^
               expr : {
                    list: {
                        name: a,
                        next: { 
                            name: b,
                            next: {
                                name: c
                                next: NULL
                            }
                        }
                    }
                    index : {
                        1000
                    }
                }
                */
              
        struct knit_index { 
            struct knit_expr *list;
            struct knit_expr *index; /*the left most item of child must be a Name*/
        } index;
        struct knit_call call;
        struct knit_kfunc *kfunc;

        /*
        KAX_CALL: call
        KAX_ASSIGNMENT: bin
        KAX_LITERAL_STR: str
        KAX_LITERAL_INT: integer
        KAX_LITERAL_LIST: list
        KAX_FUNCTION: kfunc
        KAX_VAR_REF: varref
        KAX_LIST_INDEX: index
        KAX_LIST_SLICE: slice
        KAX_OBJ_DOT: prefix
        KAX_BIN_OP: bin
        KAX_UN_OP: un
        */

    } u;
};


/* Stack Layout
   ARGN ..
*  ARG2
*  ARG1
*  FUNCTION
*  LOCAL1 <<BSP
*  LOCAL2
*  LOCALN ..
*  TEMP1
*  TEMP2
*  TEMPN ..
*/

enum KEMIT_VAL {
    KEMTRUE,
    KEMFALSE,
    KEMNULL,
};

//order tied to knit_insninfo
enum KNIT_INSN {
    /*
        s: stack
        t: current stack size (s[t-1] is top of stack)
     */
    KPUSH = 1, /*inputs: (offset,)                       op: s[t] = s[t-offset]; t++;*/
    KPOP,      /*inputs: (count,)                       op: t -= count;*/
    //load block.constants[index]
    KCLOAD,     /*inputs: (index,)                       op: s[t] = current_block_constants[index]; t++;*/
    //load block.constants[index]
    
    K_GLB_LOAD, /*inputs: (none)                       op: s[t-1] = globals[s[t-1]] */
    K_GLB_STORE,/*inputs: (none)                       op: globals[s[t-2]] = s[t-1] */

    KCALL,     /*inputs: (nargs, nexpected)       op: s[t-1](args...)*/
    KINDX,     /*inputs: (container_index, expr_index)  op: s[t] = (s[container_index])[s[expr_index]]; t++*/
    KRET,      /*inputs: (count,)  op: s[prev_bsp : prev_bsp + count] = s[t-count : t] t = prev_bsp + count;*/

    KJMP,      /*inputs: (address,)  op: IP = address;*/
    KJMPTRUE,  /*inputs: (address,)  op: if (runtime.last_condition) IP = address;*/
    KJMPFALSE, /*inputs: (address,)  op: if !(runtime.last_condition) IP = address;*/

    KTESTEQ,    /*inputs: (none)  (runtime.last_condition) = s[t-2] == s[t-1]; pop2 */
    KTESTNEQ,    /*inputs:(none)  (runtime.last_condition) = s[t-2] == s[t-1]; pop2 */
    KTESTGT,    /*inputs: (none)  (runtime.last_condition) = s[t-2] == s[t-1]; pop2 */
    KTESTLT,    /*inputs: (none)  (runtime.last_condition) = s[t-2] == s[t-1]; pop2 */
    KTESTGTEQ,  /*inputs: (none)  (runtime.last_condition) = s[t-2] == s[t-1]; pop2 */
    KTESTLTEQ,  /*inputs: (none)  (runtime.last_condition) = s[t-2] == s[t-1]; pop2 */
    KTESTNOT,   /*inputs: (none)  (runtime.last_condition) = !(runtime.last_condition)*/
    KTEST,      /*inputs: (none)  (runtime.last_condition) = bool (s[t-1]); pop1*/

    KSAVETEST,  /*inputs: (none)  push runtime.last_condition; */

    KSET,      /*inputs: (none)  op: *s[t-2] = s[t-1]; pop(2);*/

    KLLOAD,      /*inputs: (idx)  op: s[t] = s[bsp + idx];*/
    KLSTORE,     /*inputs: (idx)  op: s[bsp + idx] = s[t-1]; pop(1);*/

    KEMIT,  /*inputs (value type: (true|false|null)) push value;*/

    KADD,  /*s[t-2] = s[t-2] + s[t-1]; pop 1;*/
    KSUB,  /*s[t-2] = s[t-2] - s[t-1]; pop 1;*/
    KMUL,  /*s[t-2] = s[t-2] * s[t-1]; pop 1;*/
    KDIV,  /*s[t-2] = s[t-2] / s[t-1]; pop 1;*/
    KMOD,  /*s[t-2] = s[t-2] % s[t-1]; pop 1;*/
};
#define KINSN_FIRST KPUSH
#define KINSN_LAST  KMOD
#define KINSN_TVALID(type)  ((type) >= KINSN_FIRST && (type) <= KINSN_LAST)

//Order is tied to enum
static struct knit_insninfo {
    int insn_type;
    const char *rep;
    int n_op; //number of operands (used for dumping)
} knit_insninfo[] = {
    {0, NULL, 0},
    {KPUSH, "KPUSH", 1},
    {KPOP,  "KPOP",   1},
    {KCLOAD, "KCLOAD", 1},
    {K_GLB_LOAD, "K_GLB_LOAD", 0},
    {K_GLB_STORE, "K_GLB_STORE", 0},
    {KCALL, "KCALL", 2},
    {KINDX, "KINDX", 2},
    {KRET,  "KRET", 1},

    {KJMP,   "KJMP", 1},
    {KJMPTRUE,  "KJMPTRUE", 1},
    {KJMPFALSE, "KJMPFALSE", 1},
    {KTESTEQ,  "KTESTEQ", 0},
    {KTESTNEQ, "KTESTNEQ", 0},
    {KTESTGT,  "KTESTGT", 0},
    {KTESTLT,  "KTESTLT", 0},
    {KTESTGTEQ,  "KTESTGTEQ", 0},
    {KTESTLTEQ,  "KTESTLTEQ", 0},
    {KTESTNOT,   "KTESTNOT", 0},
    {KTEST,      "KTEST", 0},
    {KSAVETEST,  "KSAVETEST", 0},

    {KSET,  "KSET", 0},
    {KLLOAD,  "KLLOAD",  1},
    {KLSTORE, "KLSTORE", 1},
    {KEMIT, "KEMIT", 1},
    {KADD,  "KADD",   0},
    {KSUB,  "KSUB",   0},
    {KMUL,  "KMUL",   0},
    {KDIV,  "KDIV",   0},
    {KMOD,  "KMOD",   0},
    {0, NULL, 0},
};
struct knit_lex {
    struct knit_str *filename;
    struct knit_str *input;
    int lineno;
    int colno;
    int offset;
    int tokno; //refers to an index in .tokens

    struct tok_darr tokens;
};

enum KNIT_VARLOC {
    //non-negative integers mean local variables
    KLOC_GLOBAL_R,  //implicit reference to a global readonly
    KLOC_GLOBAL_RW, //implicit reference to a global readwrite
    KLOC_LOCAL_VAR, 
    KLOC_ARG, 
};
struct knit_varname {
    struct knit_str name;
    int location;
    int idx; //only relevent if location == KLOC_LOCAL_VAR || location == KLOC_ARG
};
#include "knit_varname_darr.h"

struct knit_curblk {
    struct knit_block block; //the current block we're constructing
    struct knit_varname_darr locals; //the names of locals in current block
    struct knit_expr expr; //a temporary place to store the current expression/statement in, later it is saved during parsing
    struct knit_curblk *parent; //parent must outlive child
};

//consequences of changes:
//  direct access to 
//  prs->block  =>  prs->curblk->block
//  prs->expr  =>  prs->curblk->expr
struct knit_prs {
    struct knit_lex lex; //fwd
    struct knit_curblk *curblk;
};
