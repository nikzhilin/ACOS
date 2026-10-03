#include "post.h"
#include "rng.h"

void PostInit(Post *p) {
    p->car = -1;
    p->left = 0;
    p->duration = 0;
    p->repair = 0;
}

PostState PostStateOf(const Post *p) {
    if (p->repair > 0) {
        return POST_BROKEN;
    }
    if (p->car < 0) {
        return POST_IDLE;
    }
    if (p->left > 0) {
        return POST_BUSY;
    }
    return POST_BLOCKED;
}

int PostTryBreak(Post *p, double prob, int rmin, int rmax) {
    if (p->repair == 0 && prob > 0 && RngChance(prob)) {
        p->repair = RngRange(rmin, rmax);
        return p->repair;
    }
    return 0;
}

int PostRepairStep(Post *p) {
    if (p->repair > 0) {
        return --p->repair == 0;
    }
    return 0;
}

int PostWork(Post *p) {
    if (p->left > 0) {
        return --p->left == 0;
    }
    return 0;
}

void PostStart(Post *p, int car, int duration) {
    p->car = car;
    p->left = duration;
    p->duration = duration;
}

int PostRelease(Post *p) {
    int car = p->car;
    p->car = -1;
    p->left = 0;
    p->duration = 0;
    return car;
}
