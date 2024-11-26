#include <linux/i2c.h>
#include <linux/types.h>
#include "tuner-i2c.h"
#include "mxl603_api.h"
#include "mxl603-tuner.h"

struct mxl603_state {
	struct mxl603_config *config;
	struct i2c_adapter   *i2c;
	u8 addr;
	u32 frequency;
	u32 bandwidth;
};

static int mxl603_synth_lock_status(struct mxl603_state *state, int *rf_locked, int *ref_locked)
{
	u8 d = 0;
	int ret;
	MXL_BOOL rfLockPtr, refLockPtr;

	*rf_locked = 0;
	*ref_locked = 0;

//	ret = mxl603_read_reg(state, 0x2B, &d);
	ret = MxLWare603_API_ReqTunerLockStatus(state->i2c, state->addr, &rfLockPtr, &refLockPtr);
dev_info(&state->i2c->dev, "rf_locked=%d ref_locked=%d", rfLockPtr, refLockPtr);
	if (ret)
		goto err;

	if (rfLockPtr)
		*rf_locked = 1;

	if (refLockPtr)
		*ref_locked = 1;
	
	return 0;
	
err:
	dev_dbg(&state->i2c->dev, "%s: failed=%d\n", __func__, ret);
	return ret;
}

static int mxl603_get_status(struct dvb_frontend *fe, u32 *status)
{
	struct mxl603_state *state = fe->tuner_priv;
	int rf_locked, ref_locked, ret;

	*status = 0;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	ret = mxl603_synth_lock_status(state, &rf_locked, &ref_locked);
	if (ret)
		goto err;

	dev_info(&state->i2c->dev, "%s%s", rf_locked ? "rf locked " : "",
			ref_locked ? "ref locked" : "");

	if ((rf_locked) || (ref_locked))
		*status |= TUNER_STATUS_LOCKED;

		
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	return 0;
	
err:
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
		
	dev_dbg(&state->i2c->dev, "%s: failed=%d\n", __func__, ret);
	return ret;
}

static int mxl603_get_rf_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct mxl603_state *state = fe->tuner_priv;
	SINT16 rxPwrPtr;
	int ret;
	
	*strength = 0;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	ret = MxLWare603_API_ReqTunerRxPower(state->i2c, state->addr, &rxPwrPtr);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
	
	if (!ret)
	{
		*strength = - rxPwrPtr/100;	
//		dev_info(&state->i2c->dev, "rxPwrPtr=%d strength=%d\n", rxPwrPtr, *strength);
	}
	else
		dev_dbg(&state->i2c->dev, "%s: failed=%d\n", __func__, ret);
		
	return ret;
}

static int mxl603_set_params(struct dvb_frontend *fe)
{	
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct mxl603_state *state = fe->tuner_priv;	
	MXL603_XTAL_SET_CFG_T xtalCfg;
	MXL603_IF_OUT_CFG_T ifOutCfg;
	MXL603_AGC_CFG_T agcCfg;
	MXL603_TUNER_MODE_CFG_T tunerModeCfg;
	MXL603_BW_E bandWidth;
	int ret;
	int rf_locked, ref_locked;
	u32 freq = c->frequency;
	
	dev_info(&state->i2c->dev, 
		"%s: delivery_system=%d frequency=%d bandwidth_hz=%d\n", 
		__func__, c->delivery_system, c->frequency, c->bandwidth_hz);		
			

	switch (c->delivery_system) {
	case SYS_ATSC:
		tunerModeCfg.signalMode = MXL603_DIG_ISDBT_ATSC;
		bandWidth = MXL603_TERR_BW_6MHz ;
//		ftable = MxL603_Digital;
		break;
	case SYS_DVBC_ANNEX_A:
		tunerModeCfg.signalMode = MXL603_DIG_DVB_C;
//		ftable = MxL603_Cable;
		bandWidth = MXL603_CABLE_BW_8MHz;	
		break;
	case SYS_DVBT:
	case SYS_DVBT2:
		tunerModeCfg.signalMode = MXL603_DIG_DVB_T_DTMB;
//		ftable = MxL603_Digital;
		switch (c->bandwidth_hz) {
		case 6000000:
			bandWidth = MXL603_TERR_BW_6MHz;
			break;
		case 7000000:
			bandWidth = MXL603_TERR_BW_7MHz ;
			break;
		case 8000000:
			bandWidth = MXL603_TERR_BW_8MHz ;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		 dev_dbg(&state->i2c->dev, "%s: err state=%d\n", 
			__func__, fe->dtv_property_cache.delivery_system);
		return -EINVAL;
	}

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	
//	ret = mxl603_tuner_init_default(state);
//	if (ret)
//		goto err;

//	ret = mxl603_set_xtal(state);
	/* Single XTAL for tuner and demod sharing*/
/*	xtalCfg.xtalFreqSel = MXL603_XTAL_24MHz;
	xtalCfg.xtalCap = 12; //Pls. set this based on the XTAL's SPEC : the matching capacitence to output accurate Clock
	xtalCfg.clkOutEnable = MXL_ENABLE;
	xtalCfg.clkOutDiv = MXL_DISABLE;
	xtalCfg.clkOutExt = MXL_DISABLE;
	xtalCfg.singleSupply_3_3V = MXL_ENABLE;
	xtalCfg.XtalSharingMode = MXL_DISABLE;
	ret = MxLWare603_API_CfgDevXtal(state->i2c, state->addr, xtalCfg);
	if (ret)
		goto err;
		
	ret = MxLWare603_API_CfgTunerLoopThrough(state->i2c, state->addr, MXL_DISABLE);*/

//	ret = mxl603_set_if_out(state);
	//IF freq set, should match Demod request 
	ifOutCfg.ifOutFreq = MXL603_IF_5MHz; //we suggest 5Mhz for ATSC MN88436 
	ifOutCfg.ifInversion = MXL_ENABLE;
	ifOutCfg.gainLevel = 11;
	ifOutCfg.manualFreqSet = MXL_DISABLE;
	ifOutCfg.manualIFOutFreqInKHz = 5000;//4984;
	ret = MxLWare603_API_CfgTunerIFOutParam(state->i2c, state->addr, ifOutCfg);
	if (ret)
		goto err;

//	ret = mxl603_set_agc(state);
	//agcCfg.agcType = MXL603_AGC_SELF; //if you doubt DMD IF-AGC part, pls. use Tuner self AGC instead.
/*	agcCfg.agcType = MXL603_AGC_EXTERNAL;
	agcCfg.setPoint = 66;
	agcCfg.agcPolarityInverstion = MXL_DISABLE;
	ret = MxLWare603_API_CfgTunerAGC(state->i2c, state->addr, agcCfg);
	if (ret)
		goto err;*/

//	ret = mxl603_set_mode(fe, state, mode);
	//Step 6 : Application Mode setting

	//IF freq set, should match Demod request 
	tunerModeCfg.ifOutFreqinKHz = 5000;

	/* Single XTAL for tuner and demod sharing*/
	tunerModeCfg.xtalFreqSel = MXL603_XTAL_24MHz;
	tunerModeCfg.ifOutGainLevel = 11;
	ret = MxLWare603_API_CfgTunerMode(state->i2c, state->addr, tunerModeCfg);
	if (ret)
		goto err;
		
//	ret = mxl603_set_freq(state, freq, mode, bw, ftable);
	Mxl603SetFreqBw(state->i2c, state->addr, freq, bandWidth, tunerModeCfg.signalMode);
	if (ret)
		goto err;

	state->frequency = freq;
	state->bandwidth = c->bandwidth_hz;

	msleep(20);
	ret = mxl603_synth_lock_status(state, &rf_locked, &ref_locked);
//dev_info(&state->i2c->dev, "rf_locked=%d ref_locked=%d", rf_locked, ref_locked);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
		
	msleep(15);
		
	return 0;
	
err:
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
		
	dev_dbg(&state->i2c->dev, "%s: failed=%d\n", __func__, ret);
	return ret;
}

static int mxl603_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct mxl603_state *state = fe->tuner_priv;
	*frequency = state->frequency;
	return 0;
}

static int mxl603_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct mxl603_state *state = fe->tuner_priv;
	*bandwidth = state->bandwidth;
	return 0;
}

static int mxl603_tuner_init(struct dvb_frontend *fe)
{
	struct mxl603_state *state = fe->tuner_priv;
	int ret;
	MXL603_VER_INFO_T	mxl603Version;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	ret = MXL603_init(state->i2c, state->addr);
//	ret = MxLWare603_API_CfgDevPowerMode(state->i2c, state->addr, MXL603_PWR_MODE_ACTIVE);
	
//	msleep(15);
//	MxLWare603_API_ReqDevVersionInfo(state->i2c, state->addr, &mxl603Version);
//dev_info(&state->i2c->dev, "chipId=%02x chipVersion=%02x mxlwareVer=%d.%d.%d.%d.%d",
//	mxl603Version.chipId, mxl603Version.chipVersion, mxl603Version.mxlwareVer[0],
//	mxl603Version.mxlwareVer[1], mxl603Version.mxlwareVer[2], mxl603Version.mxlwareVer[3], mxl603Version.mxlwareVer[4]);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	return 0;
	
err:
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
		
	dev_dbg(&state->i2c->dev, "%s: failed=%d\n", __func__, ret);
	return ret;
}

static int mxl603_sleep(struct dvb_frontend *fe)
{
	struct mxl603_state *state = fe->tuner_priv;
	int ret;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	/* enter standby mode */
	ret = MxLWare603_API_CfgDevPowerMode(state->i2c, state->addr, MXL603_PWR_MODE_STANDBY);
	
	if (ret)
		goto err;
		
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	return 0;
	
err:
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
		
	dev_dbg(&state->i2c->dev, "%s: failed=%d\n", __func__, ret);
	return ret;
}

void mxl603_release(struct dvb_frontend *fe)
{
	struct mxl603_state *state = fe->tuner_priv;

	fe->tuner_priv = NULL;
	kfree(state);
	
	return;
}

static struct dvb_tuner_ops mxl603_tuner_ops = {
	.info = {
		.name = "MaxLinear MxL603",
		.frequency_min_hz = 1 * MHz,
		.frequency_max_hz = 1200 * MHz,
		.frequency_step_hz = 25 * kHz,
	},
	.init              = mxl603_tuner_init,
	.sleep             = mxl603_sleep,
	.set_params        = mxl603_set_params,
	.get_status        = mxl603_get_status,
	.get_rf_strength   = mxl603_get_rf_strength,
	.get_frequency     = mxl603_get_frequency,
	.get_bandwidth     = mxl603_get_bandwidth,
	.release           = mxl603_release,
//	.get_if_frequency  = mxl603_get_if_frequency,
};

struct dvb_frontend *mxl603_attach(struct dvb_frontend *fe,
				     struct i2c_adapter *i2c, u8 addr,
				     struct mxl603_config *config)
{
	struct mxl603_state *state = NULL;
	int ret = 0;
	MXL603_VER_INFO_T	mxl603Version;

	state = kzalloc(sizeof(struct mxl603_state), GFP_KERNEL);
	if (!state) {
		ret = -ENOMEM;
		dev_err(&i2c->dev, "kzalloc() failed\n");
		goto err1;
	}
	
	state->config = config;
	state->i2c = i2c;
	state->addr = addr;
	
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
		
	ret = MxLWare603_API_CfgDevSoftReset(state->i2c, state->addr);

//	ret = mxl603_get_chip_id(state);
	ret |= MxLWare603_API_ReqDevVersionInfo(state->i2c, state->addr, &mxl603Version);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	dev_info(&state->i2c->dev, "MxL603 detected id(%02x) ver(%02x)\n", mxl603Version.chipId, mxl603Version.chipVersion);

	/* check return value of mxl603_get_chip_id */
	if (ret)
		goto err2;
	
	dev_info(&i2c->dev, "Attaching MxL603\n");
	
	fe->tuner_priv = state;

	memcpy(&fe->ops.tuner_ops, &mxl603_tuner_ops,
	       sizeof(struct dvb_tuner_ops));

	return fe;
	
err2:
	kfree(state);
err1:
	return NULL;
}
EXPORT_SYMBOL(mxl603_attach);

MODULE_DESCRIPTION("MaxLinear MxL603 tuner driver");
MODULE_AUTHOR("Sasa Savic <sasa.savic.sr@gmail.com>");
MODULE_LICENSE("GPL");