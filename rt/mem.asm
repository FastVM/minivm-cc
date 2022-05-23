
func lib.pool.size
    r0 <- uint 16
    ret r0
end

func lib.pool.get
    r4 <- call lib.pool.size
    r3 <- uint 1
@lib.pool.get.loop
    ubb r4 lib.pool.get.loop.zero lib.pool.get.loop.nonzero
@lib.pool.get.loop.zero
    r1 <- getcar r1
    ret r1
@lib.pool.get.loop.nonzero
    r0 <- uint 1
    r4 <- usub r4 r0
    r5 <- reg r3
    r3 <- uadd r3 r3
    r0 <- umod r2 r3
    ublt r0 r5 lib.pool.get.car lib.pool.get.cdr
@lib.pool.get.car
    r1 <- getcar r1
    jump lib.pool.get.loop
@lib.pool.get.cdr
    r1 <- getcdr r1
    jump lib.pool.get.loop
end

func lib.pool.set
    r4 <- call lib.pool.size
    r5 <- uint 1
@lib.pool.set.loop
    ubb r4 lib.pool.set.loop.zero lib.pool.set.loop.nonzero
@lib.pool.set.loop.zero
    setcar r1 r3
    ret r3
@lib.pool.set.loop.nonzero
    r0 <- uint 1
    r4 <- usub r4 r0
    r6 <- reg r5
    r5 <- uadd r5 r5
    r0 <- umod r2 r5
    ublt r0 r6 lib.pool.set.car lib.pool.set.cdr
@lib.pool.set.car
    r1 <- getcar r1
    jump lib.pool.set.loop
@lib.pool.set.cdr
    r1 <- getcdr r1
    jump lib.pool.set.loop
end

func lib.pool.new
    ubb r1 lib.pool.new.zero lib.pool.new.nonzero
@lib.pool.new.zero
    r0 <- uint 0
    r0 <- cons r0 r0
    ret r0
@lib.pool.new.nonzero
    r0 <- uint 1
    r1 <- usub r1 r0
    r2 <- call lib.pool.new r1
    r3 <- call lib.pool.new r1
    r0 <- cons r2 r3
    ret r0
end

func lib.pool.init
    r0 <- call lib.pool.size
    r0 <- call lib.pool.new r0
    ret r0
end