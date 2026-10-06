#include "ledgedash_logic.h"
#include "../MexTK/mex.h"
int Ldsh_LegacyResetFailure(int state, int state_frame, int dead, int grounded, int released, int pending_hard) {
    return dead || (state == ASID_ESCAPEAIR && state_frame >= 9) ||
        (state >= ASID_CLIFFCLIMBSLOW && state <= ASID_CLIFFJUMPQUICK2) ||
        (!pending_hard && grounded && released && state != ASID_LANDING &&
         state != ASID_LANDINGFALLSPECIAL && state != ASID_REBIRTHWAIT && state_frame >= 12);
}
int Ldsh_RandomDistance(int low, int high, unsigned roll) {
    if (low > high) { int temp = low; low = high; high = temp; }
    return low + roll % (unsigned)(high - low + 1);
}
int Ldsh_IsAttackDash(int attack_kind, int state) {
    return state == ASID_DASH || attack_kind != 1 || state == ASID_CATCH || state == ASID_CATCHDASH;
}
void LdshAttempt_Reset(LdshAttempt *a) { *a = (LdshAttempt){0}; }
static int resolve(LdshAttempt *a, int result) {
    a->result = result; a->phase = LDSH_RESOLVED;
    return result;
}
int LdshAttempt_Step(LdshAttempt *a, const LdshSample *s, unsigned criterion) {
    if (a->phase == LDSH_RESOLVED) return a->result;
    if (criterion >= LDSH_CRITERIA_COUNT) criterion = LDSH_GALINT;
    if (a->phase == LDSH_APPROACH) {
        if (s->on_ledge) a->phase = LDSH_ON_LEDGE;
        return LDSH_PENDING; /* Setup/recovery inputs are not ledgedash attempts. */
    }
    /* egg_pop is captured at the protected damage collision, not retirement or
     * the following update (which can already have exhausted the timer). */
    if (criterion == LDSH_POP_EGG && a->landing_complete && s->egg_pop)
        return resolve(a, LDSH_SUCCESS);
    if (s->frozen) return LDSH_PENDING;
    if (a->phase == LDSH_ON_LEDGE) {
        if (s->ledge_option) return resolve(a, LDSH_FAILURE);
        if (!s->released) return LDSH_PENDING;
        a->phase = LDSH_RELEASED;
    }
    if (criterion == LDSH_POP_EGG && !s->target_ready && !s->egg_pop)
        return resolve(a, LDSH_SETUP_UNAVAILABLE);
    a->elapsed++;
    if (s->dead || s->ledge_option || a->elapsed > 180) return resolve(a, LDSH_FAILURE);
    if (s->airdodge) a->airdodged = 1;
    if (s->grounded) {
        a->phase = LDSH_LANDED;
        if (a->airdodged) a->wavelanded = 1;
    }
    /* An immediate jump on the first recovered frame must still resolve the
     * original GALINT criterion after observed ground contact. */
    if (a->phase == LDSH_LANDED && s->actionable && !a->landing_complete) {
        a->landing_complete = 1;
        a->galint_at_landing = s->galint;
    }
    if (criterion == LDSH_WAVELAND && a->wavelanded) return resolve(a, LDSH_SUCCESS);
    if (!a->landing_complete) return LDSH_PENDING;
    if (criterion == LDSH_WAVELAND) return resolve(a, LDSH_FAILURE);
    if (criterion == LDSH_GALINT)
        return resolve(a, a->galint_at_landing > 0 ? LDSH_SUCCESS : LDSH_FAILURE);
    if (criterion == LDSH_POP_EGG && !s->target_ready) return resolve(a, LDSH_SETUP_UNAVAILABLE);
    if (s->grounded && s->galint > 0 &&
        ((criterion == LDSH_ACT_GALINT && s->attack_dash) || (criterion == LDSH_POP_EGG && s->egg_pop)))
        return resolve(a, LDSH_SUCCESS);
    if (s->galint <= 0) return resolve(a, LDSH_FAILURE);
    return LDSH_PENDING;
}
int LdshSurface_Target(const LdshSurface *s, float desired_x, float ledge_y,
                       int platform, float *x, float *y) {
    if (!s->enabled || !!s->drop != !!platform) return 0;
    float left = s->x1 < s->x2 ? s->x1 : s->x2;
    float right = s->x1 > s->x2 ? s->x1 : s->x2;
    if (right - left < 6) return 0;
    *x = desired_x < left + 3 ? left + 3 : desired_x > right - 3 ? right - 3 : desired_x;
    *y = s->y1 + (*x - s->x1) * (s->y2 - s->y1) / (s->x2 - s->x1);
    if (platform ? *y < ledge_y + 4 : *y > ledge_y + 20) return 0;
    return 1;
}
