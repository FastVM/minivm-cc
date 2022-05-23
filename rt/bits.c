
int __minivm_bits_shl(int val, int bits) {
    while (bits > 0) {
        bits -= 1;
        val += val;
    }
    return val;
}

int __minivm_bits_shr(int val, int bits) {
    return val / __minivm_bits_shl(1, bits);
}

int __minivm_bits_xor(int lhs, int rhs) {
    int ret = 0;
    int bit = 1;
    while (1) {
        int nextbit = bit + bit;
        if (bit > lhs && bit > rhs) {
            return ret;
        }
        if ((lhs % bit > nextbit) != (rhs % bit > nextbit)) {
            ret += bit;
        }
        bit = nextbit;
    }
}

int __minivm_bits_or(int lhs, int rhs) {
    int ret = 0;
    int bit = 1;
    while (1) {
        int nextbit = bit + bit;
        if (bit > lhs && bit > rhs) {
            return ret;
        }
        if ((lhs % bit > nextbit) || (rhs % bit > nextbit)) {
            ret += bit;
        }
        bit = nextbit;
    }
}

int __minivm_bits_and(int lhs, int rhs) {
    int ret = 0;
    int bit = 1;
    while (1) {
        int nextbit = bit + bit;
        if (bit > lhs && bit > rhs) {
            return ret;
        }
        if ((lhs % bit > nextbit) && (rhs % bit > nextbit)) {
            ret += bit;
        }
        bit = nextbit;
    }
}
