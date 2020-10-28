#ifndef THREADS_CALCULATE_H
#define THREADS_CALCULATE_H

#define FL(X) (int)(X<<16)

#define LF(X) (int)(X>>16)

/* Multiply two fixed-point value. */
#define F_MULT(A,B) ((int)(((int64_t) A) * B >> 16))
/* Divide two fixed-point value. */
#define F_DIV(A,B) ((int)((((int64_t) A) << 16) / B))

#define F_ROUND(X) (X >= 0 ? ((X + (1 << (16 - 1))) >> 16) : ((X - (1 << (16 - 1))) >> 16))

#endif /* threads/calculate.h */