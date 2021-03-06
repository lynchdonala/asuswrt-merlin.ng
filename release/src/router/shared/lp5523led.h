#ifndef _LP5523LED_H_
#define _LP5523LED_H_

#if defined(RTCONFIG_LP5523)

struct lp55xx_leds_pattern {
	int ptn_mode;
	char *ptn1;
	char *ptn2;
	char *ptn3;
};

enum lp55xx_engine_index {
	LP55XX_ENGINE_INVALID = 0,
	LP55XX_ENGINE_1,
	LP55XX_ENGINE_2,
	LP55XX_ENGINE_3,
	LP55XX_ENGINE_MAX = LP55XX_ENGINE_3 +1
};

enum lp55xx_leds_mode {
	LP55XX_START = 0,
//	Color
	LP55XX_ALL_LEDS_ON,		//	(1)
	LP55XX_ALL_LEDS_OFF,		//	(2)
	LP55XX_ALL_BLEDS_ON,		//	(3)
	LP55XX_ALL_GLEDS_ON,		//	(4)
	LP55XX_ALL_RLEDS_ON,		//	(5)
	LP55XX_ALL_BREATH_LEDS = 100,	//	(100)
	LP55XX_BLACK_LEDS,		//	(101)
	LP55XX_WHITE_LEDS,		//	(102)
	LP55XX_RED_LEDS,		//	(103)
	LP55XX_LIGHT_CYAN_LEDS,		//	(104)
	LP55XX_PURPLE_LEDS,		//	(105)
	LP55XX_NIAGARA_BLUE_LEDS,	//	(106)
	LP55XX_PALE_DOGWOOD_LEDS,	//	(107)
	LP55XX_PRIMROSE_YELLOW_LEDS,	//	(108)
	LP55XX_BTCOR_LEDS,		//	(109)
	LP55XX_LINKCOR_LEDS,		//	(110)
	LP55XX_ORANGE_LEDS,		//	(111)
	LP55XX_GREENERY_LEDS,		//	(112)
	LP55XX_BLUEGREEN_BREATH,	//	(113)
	LP55XX_WPS_SYNC_LEDS,		//	(114)
	LP55XX_AMAS_ETH_LINK_LEDS,	//	(115)
	LP55XX_AMAS_RE_SYNC_LEDS,	//	(116)
	LP55XX_AMAS_CAPAP_LEDS,		//	(117)
	LP55XX_DISCONNCOR_LDES,		//	(118)
	LP55XX_AMAS_REJOIN_LDES,	//	(119)
	LP55XX_END_COLOR,
//	Behavior of ACT
	LP55XX_PREVIOUS_STATE = 200,
	LP55XX_WPS_TRIG,
	LP55XX_WPS_SUCCESS,
	LP55XX_RESET_TRIG,
	LP55XX_RESET_SUCCESS,
	LP55XX_WIFI_PARAM_SYNC,
	LP55XX_WPS_PARAM_SYNC,
	LP55XX_SCH_ENABLE,
	LP55XX_MANUAL_BREATH,
	LP55XX_MANUAL_COL,
	LP55XX_END_ACT,
//	Behavior of Blink Mode
	LP55XX_ACT_NONE = 300,		//	(300)
	LP55XX_ACT_SBLINK,		//	(301)
	LP55XX_ACT_3ON1OFF,		//	(302)
	LP55XX_ACT_BREATH_UP_00,	//	(303)
	LP55XX_ACT_BREATH_DOWN_00,	//	(304)	
	LP55XX_ACT_BREATH_DOWN_01,	//	(305)
	LP55XX_ACT_BREATH_DOWN_02,	//	(306)
	LP55XX_ACT_BREATH_UP_01,	//	(307)
	LP55XX_END_BLINK,
//	Exception of Blink Mode
	LP55XX_END
};

extern void lp55xx_leds_proc(int ptv_mode, int ptb_mode);
extern void lp55xx_leds_sch(int start, int end);

#endif /* RTCONFIG_LP5523 */
#endif /* _LP5523LED_H_ */
