; ModuleID = 'test.ll'
source_filename = "test.ll"

define i32 @main() {
entry:
  %1 = call i32 @custom_entry()
  %2 = sub i32 %1, 123
  ret i32 %2
}

define i32 @custom_entry() {
entry:
  ret i32 123
}
