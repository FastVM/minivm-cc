print("#include <stdio.h>")
for i in range(100_000):
    print("int fib%i(int n) {\n    if (n < 2) {\n        return n;\n    } else {\n        return fib%i(n-1) + fib%i(n-2);\n    }\n}" % (i, i, i))
print("int main() {")
print("}")
