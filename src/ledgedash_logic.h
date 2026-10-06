#ifndef TYRO_LEDGEDASH_LOGIC_H
#define TYRO_LEDGEDASH_LOGIC_H
enum { LDSH_GALINT, LDSH_WAVELAND, LDSH_ACT_GALINT, LDSH_POP_EGG, LDSH_CRITERIA_COUNT };
enum { LDSH_APPROACH, LDSH_ON_LEDGE, LDSH_RELEASED, LDSH_LANDED, LDSH_RESOLVED };
enum { LDSH_PENDING, LDSH_SUCCESS, LDSH_FAILURE, LDSH_SETUP_UNAVAILABLE };
typedef struct LdshAttempt {
    int phase, elapsed, airdodged, wavelanded, landing_complete, galint_at_landing;
    int result, counted;
} LdshAttempt;
typedef struct LdshSample {
    int on_ledge, released, airdodge, grounded, actionable, galint;
    int attack_dash, egg_pop, dead, ledge_option, frozen, target_ready;
} LdshSample;
void LdshAttempt_Reset(LdshAttempt *attempt);
int LdshAttempt_Step(LdshAttempt *attempt, const LdshSample *sample, unsigned criterion);
typedef struct LdshSurface { float x1, y1, x2, y2; int drop, enabled; } LdshSurface;
int LdshSurface_Target(const LdshSurface *surface, float desired_x, float ledge_y,
                       int platform, float *x, float *y);
int Ldsh_IsAttackDash(int attack_kind, int state);
#endif
