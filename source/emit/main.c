#include <stdio.h>

#include "ati/basic.h"
#include <sys/mman.h>

u32 *make_code(u32 size) {
    u32 *memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_JIT, -1, 0);
    if (memory == MAP_FAILED) {
        perror("mmap");
        return null;
    }
    return memory;
}

bool exec_code(void *data, u32 size) {
    if (mprotect(data, size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect");
        return false;
    }

    return true;
}

typedef u32 (*runfunc)();

runfunc generate_runfunc() {
    u32 *code = make_code(1024), *cursor = code;
    if (code == null)
        return null;

    *cursor++ = 0x58000000;// ldr x0, 1
    *cursor++ = 0xd65f03c0;// ret

    if (!exec_code(code, 1024))
        return null;

    return (runfunc) code;
}

int main() {
    runfunc run = generate_runfunc();
    if (run == null) {
        return 1;
    }
    printf("%x\n", run());
    /*
     * jit_function_t build_gcd_func(jit_context_t context) {
jit_context_build_start(context);

  // Create function signature and object. int (*)(int, int)
  jit_type_t params[2] = {jit_type_int, jit_type_int};
  jit_type_t signature = jit_type_create_signature(
          jit_abi_cdecl, jit_type_int, params, 2, 1);
  jit_function_t F = jit_function_create(context, signature);

  // u, v are function parameters; t is a temporary value.
  jit_value_t u, v, t;
  u = jit_value_get_param(F, 0);
  v = jit_value_get_param(F, 1);
  t = jit_value_create(F, jit_type_int);

  // Create the while (v) condition with a label that allows to loop back.
  //
  // label_while:
  //   if (v == 0) goto label_after_while
  //   .. contents of while loop
  //
  // label_after_while is created as undefined at this point, so that
  // instructions can have forward references to it. It will be placed later.
  jit_label_t label_while = jit_label_undefined;
  jit_label_t label_after_while = jit_label_undefined;
  jit_value_t const0 = jit_value_create_nint_constant(F, jit_type_int, 0);

  jit_insn_label(F, &label_while);
  jit_value_t cmp_v_0 = jit_insn_eq(F, v, const0);
  jit_insn_branch_if(F, cmp_v_0, &label_after_while);

  // t = u
  jit_insn_store(F, t, u);
  // u = v
  jit_insn_store(F, u, v);

  // v = t % v
  jit_value_t rem = jit_insn_rem(F, t, v);
  jit_insn_store(F, v, rem);

  //   goto label_while
  // label_after_while:
  //   ...
  jit_insn_branch(F, &label_while);
  jit_insn_label(F, &label_after_while);

  //   if (u >= 0) goto label_positive
  //   return -u
  // label_pos:
  //   return u
  jit_label_t label_positive = jit_label_undefined;
  jit_value_t cmp_u_0 = jit_insn_ge(F, u, const0);
  jit_insn_branch_if(F, cmp_u_0, &label_positive);

  jit_value_t minus_u = jit_insn_neg(F, u);
  jit_insn_return(F, minus_u);
  jit_insn_label(F, &label_positive);
  jit_insn_return(F, u);

  jit_context_build_end(context);
  return F;
}


     */
    return 0;
}