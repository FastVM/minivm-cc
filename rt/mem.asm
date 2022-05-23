
func lib.pool.fillcar
    r0 <- getcar r1
    ubb r0 lib.pool.fillcar.empty lib.pool.fillcar.else
@lib.pool.fillcar.empty
    r0 <- uint 0
    r0 <- cons r0 r0
    setcar r1 r0
@lib.pool.fillcar.else
    ret r1

func lib.pool.fillcdr
    r0 <- getcdr r1
    ubb r0 lib.pool.fillcdr.empty lib.pool.fillcdr.else
@lib.pool.fillcdr.empty
    r0 <- uint 0
    r0 <- cons r0 r0
    setcdr r1 r0
@lib.pool.fillcdr.else
    ret r1

func lib.pool.get
    r4 <- uint 22
    r3 <- uint 1
@lib.pool.get.loop
    r0 <- uint 1
    r4 <- usub r4 r0
    ubb r4 lib.pool.get.loop.zero lib.pool.get.loop.nonzero
@lib.pool.get.loop.zero
    r5 <- reg r3
    r3 <- uadd r3 r3
    r0 <- umod r2 r3
    ublt r0 r5 lib.pool.get.retcar lib.pool.get.retcdr
@lib.pool.get.retcar
    r1 <- getcar r1
    ret r1
@lib.pool.get.retcdr
    r1 <- getcdr r1
    ret r1
@lib.pool.get.loop.nonzero
    r5 <- reg r3
    r3 <- uadd r3 r3
    r0 <- umod r2 r3
    ublt r0 r5 lib.pool.get.getcar lib.pool.get.getcdr
@lib.pool.get.getcar
    r1 <- getcar r1
    jump lib.pool.get.loop
@lib.pool.get.getcdr
    r1 <- getcdr r1
    jump lib.pool.get.loop
end

func lib.pool.set
    r4 <- uint 22
    r5 <- uint 1
@lib.pool.set.loop
    r0 <- uint 1
    r4 <- usub r4 r0
    ubb r4 lib.pool.set.loop.zero lib.pool.set.loop.nonzero
@lib.pool.set.loop.zero
    r6 <- reg r5
    r5 <- uadd r5 r5
    r0 <- umod r2 r5
    ublt r0 r6 lib.pool.set.setcar lib.pool.set.setcdr
@lib.pool.set.setcar
    setcar r1 r3
    ret r3
@lib.pool.set.setcdr
    setcdr r1 r3
    ret r3
@lib.pool.set.loop.nonzero
    r6 <- reg r5
    r5 <- uadd r5 r5
    r0 <- umod r2 r5
    ublt r0 r6 lib.pool.set.getcar lib.pool.set.getcdr
@lib.pool.set.getcar
    r0 <- call lib.pool.fillcar r1
    r1 <- getcar r1
    jump lib.pool.set.loop
@lib.pool.set.getcdr
    r0 <- call lib.pool.fillcdr r1
    r1 <- getcdr r1
    jump lib.pool.set.loop
end

func lib.pool.init
    r0 <- uint 0
    r0 <- cons r0 r0
    ret r0
end