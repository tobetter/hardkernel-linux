/*
 *  hardkernel_max98089.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <plat/gpio-cfg.h>

#include <mach/regs-clock.h>
#include <mach/gpio.h>

#include "i2s.h"
#include "s3c-i2s-v2.h"
#include "../codecs/max98088.h"

static struct platform_device *hardkernel_snd_device;

static struct snd_soc_jack jack;

static struct snd_soc_jack_pin jack_pins[] = {
	{
		.pin	= "MIC1",
		.mask	= SND_JACK_MICROPHONE,
	},
	{
		.pin	= "HPL",
		.mask	= SND_JACK_HEADPHONE | SND_JACK_MECHANICAL | SND_JACK_AVOUT,
	},
};

static struct snd_soc_jack_gpio jack_gpios[] = {
	{
		.gpio	= EXYNOS4_GPX3(0),
		.name	= "headset-jack",
		.report	= SND_JACK_HEADSET | SND_JACK_MECHANICAL | SND_JACK_AVOUT,
		.debounce_time = 200,
	},
};

//#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}
//#endif /* CONFIG_SND_SAMSUNG_I2S_MASTER */

static int hardkernel_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
					rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}

#if	0
//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
int	max98089_routing(u8 path)
{
	struct snd_soc_codec *codec = max98088_codec;
	struct max98088_priv *max98088 = snd_soc_codec_get_drvdata(codec);
	
	switch(path)
	{
		case 0 :
			max98089_disable_playback_path(codec, HP);
			max98089_set_playback_speaker(codec);
			max98088->cur_path = SPK;
			break;
		case 1 :
		case 2 :
			max98089_disable_playback_path(codec, SPK);
			max98089_set_playback_headset(codec);
			max98088->cur_path = HP;
			break;
		case 3 :
			max98089_disable_playback_path(codec, SPK);
			max98089_set_playback_headset(codec);
			max98088->cur_path = TV_OUT;
		default :
			break;
	}

	return 0;
}
EXPORT_SYMBOL(max98089_routing);
#endif

/*
 * HARDKERNEL MAX98089 DAI operations.
 */
static struct snd_soc_ops hardkernel_ops = {
	.hw_params = hardkernel_hw_params,
};

static int max98089_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	/* set endpoints to not connected */
	snd_soc_dapm_nc_pin(dapm, "RECL");
	snd_soc_dapm_nc_pin(dapm, "RECL");
	snd_soc_dapm_nc_pin(dapm, "MIC1");
	snd_soc_dapm_nc_pin(dapm, "INA1");
	snd_soc_dapm_nc_pin(dapm, "INA2");
	snd_soc_dapm_nc_pin(dapm, "INB1");
	snd_soc_dapm_nc_pin(dapm, "INB2");

	snd_soc_dapm_enable_pin(dapm, "SPKL");
	snd_soc_dapm_enable_pin(dapm, "SPKR");
	snd_soc_dapm_enable_pin(dapm, "HPL");
	snd_soc_dapm_enable_pin(dapm, "HPR");

	snd_soc_dapm_sync(dapm);

	s3c_gpio_cfgpin(jack_gpios[0].gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(jack_gpios[0].gpio, S3C_GPIO_PULL_NONE);

	/* Headset jack detection */
	ret = snd_soc_jack_new(codec, "Headset Jack",
			SND_JACK_HEADSET | SND_JACK_MECHANICAL | SND_JACK_AVOUT, &jack);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_pins(&jack, ARRAY_SIZE(jack_pins), jack_pins);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_gpios(&jack, ARRAY_SIZE(jack_gpios), jack_gpios);
	if (ret)
		return ret;

	return 0;
}

static struct snd_soc_dai_link hardkernel_dai[] = {
	{ /* Primary DAI i/f */
		.name = "MAX98089 AIF1",
		.stream_name = "Playback",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "HiFi",
		.platform_name = "samsung-audio",
		.codec_name = "max98088.0-0010",
		.init = max98089_init,
		.ops = &hardkernel_ops,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "MAX98089 AIF2",
		.stream_name = "Capture",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "HiFi",
		.platform_name = "samsung-audio",
		.codec_name = "max98088.0-0010",
		.ops = &hardkernel_ops,
	},
};

static struct snd_soc_card hardkernel = {
	.name		= "max98089",
	.owner		= THIS_MODULE,
	.dai_link	= hardkernel_dai,
	.num_links	= ARRAY_SIZE(hardkernel_dai),
};

static __devinit int hardkernel_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &hardkernel;
	int	ret;

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return	ret;
	}

	return	0;
}

static int __devexit hardkernel_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return	0;
}

static struct platform_driver hardkernel_driver = {
	.driver = {
		.name = "hardkernel-snd",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = hardkernel_probe,
	.remove = __devexit_p(hardkernel_remove),
};

module_platform_driver(hardkernel_driver);

MODULE_DESCRIPTION("ALSA SoC HARDKERNEL MAX98089");
MODULE_LICENSE("GPL");
