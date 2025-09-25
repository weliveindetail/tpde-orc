; Minimal test with bfloat → int

@const_val = global bfloat 0xR4248   ; ~3.14 in bfloat16

define i32 @main() {
entry:
  %c = load bfloat, ptr @const_val
  %i = fptosi bfloat %c to i32
  ret i32 %i
}
