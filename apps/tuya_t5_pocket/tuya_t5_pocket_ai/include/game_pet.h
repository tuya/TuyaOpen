#ifndef __GAME_PET_H__
#define __GAME_PET_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_S_HEALTH_INDEX = 0,
    PET_S_ENERGY_INDEX,
    PET_S_CLEAN_INDEX,
    PET_S_HAPPINESS_INDEX,
    PET_STATE_TOTAL,
} game_pet_state_id_t;

typedef enum {
    PET_O_PLAY_INDEX = 0,
    PET_O_EAT_INDEX,
    PET_O_SHOWER_INDEX,
    PET_O_SLEEP_INDEX,
    PET_O_HEALING_INDEX,
    PET_O_TIMER_INDEX,
    PET_OPT_TOTAL,
} game_pet_opt_id_t;

/**
 * @brief game pet operation function
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 */
OPERATE_RET game_pet_operation(game_pet_opt_id_t idx);

/**
 * @brief game pet init function
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 */
OPERATE_RET game_pet_init(void);

#ifdef __cplusplus
}
#endif

#endif // __GAME_PET_H__
