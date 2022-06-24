
const more = (n) => {
    return fib(n-1) + fib(n-2);
};

const fib = (n) => {
    if (n < 2) {
        return n;
    } else {
        return more(n);
    }
};

console.log(fib(40));
