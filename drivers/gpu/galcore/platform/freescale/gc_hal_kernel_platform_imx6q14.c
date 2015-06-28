/****************************************************************************
*
*    Copyright (C) 2005 - 2014 by Vivante Corp.
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****************************************************************************/


#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"
#include "gc_hal_kernel_device.h"
#include "gc_hal_driver.h"
#include <linux/slab.h>

#if USE_PLATFORM_DRIVER
#   include <linux/platform_device.h>
#endif

#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_MXC_BUSFREQ
#include <linux/busfreq-imx6.h>
#endif


#ifdef CONFIG_DEVICE_THERMAL
#include <linux/device_cooling.h>
#define REG_THERMAL_NOTIFIER(a) register_devfreq_cooling_notifier(a);
#define UNREG_THERMAL_NOTIFIER(a) unregister_devfreq_cooling_notifier(a);
#else
#define REG_THERMAL_NOTIFIER(a);
#define UNREG_THERMAL_NOTIFIER(a);
#endif

#if gcdENABLE_FSCALE_VAL_ADJUST
static int initgpu3DMinClock = 1;
module_param(initgpu3DMinClock, int, 0644);
#endif

struct platform_device *pdevice;

#ifdef CONFIG_GPU_LOW_MEMORY_KILLER
#    include <linux/kernel.h>
#    include <linux/mm.h>
#    include <linux/oom.h>
#    include <linux/sched.h>

struct task_struct *lowmem_deathpending;

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_nb = {
	.notifier_call	= task_notify_func,
};

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;

	if (task == lowmem_deathpending)
		lowmem_deathpending = NULL;

	return NOTIFY_OK;
}

extern struct task_struct *lowmem_deathpending;
static unsigned long lowmem_deathpending_timeout;

static int force_contiguous_lowmem_shrink(IN gckKERNEL Kernel)
{
	struct task_struct *p;
	struct task_struct *selected = NULL;
	int tasksize;
        int ret = -1;
	int min_adj = 0;
	int selected_tasksize = 0;
	int selected_oom_adj;
	/*
	 * If we already have a death outstanding, then
	 * bail out right away; indicating to vmscan
	 * that we have nothing further to offer on
	 * this pass.
	 *
	 */
	if (lowmem_deathpending &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout))
		return 0;
	selected_oom_adj = min_adj;

       rcu_read_lock();
	for_each_process(p) {
		struct mm_struct *mm;
		struct signal_struct *sig;
                gcuDATABASE_INFO info;
		int oom_adj;

		task_lock(p);
		mm = p->mm;
		sig = p->signal;
		if (!mm || !sig) {
			task_unlock(p);
			continue;
		}
		oom_adj = sig->oom_score_adj;
		if (oom_adj < min_adj) {
			task_unlock(p);
			continue;
		}

		tasksize = 0;
		task_unlock(p);
               rcu_read_unlock();

		if (gckKERNEL_QueryProcessDB(Kernel, p->pid, gcvFALSE, gcvDB_VIDEO_MEMORY, &info) == gcvSTATUS_OK){
			tasksize += info.counters.bytes / PAGE_SIZE;
		}
		if (gckKERNEL_QueryProcessDB(Kernel, p->pid, gcvFALSE, gcvDB_CONTIGUOUS, &info) == gcvSTATUS_OK){
			tasksize += info.counters.bytes / PAGE_SIZE;
		}

               rcu_read_lock();

		if (tasksize <= 0)
			continue;

		gckOS_Print("<gpu> pid %d (%s), adj %d, size %d \n", p->pid, p->comm, oom_adj, tasksize);

		if (selected) {
			if (oom_adj < selected_oom_adj)
				continue;
			if (oom_adj == selected_oom_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_adj = oom_adj;
	}
	if (selected) {
		gckOS_Print("<gpu> send sigkill to %d (%s), adj %d, size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_adj, selected_tasksize);
		lowmem_deathpending = selected;
		lowmem_deathpending_timeout = jiffies + HZ;
		force_sig(SIGKILL, selected);
		ret = 0;
	}
       rcu_read_unlock();
	return ret;
}


gceSTATUS
_ShrinkMemory(
    IN gckPLATFORM Platform
    )
{
    struct platform_device *pdev;
    gckGALDEVICE galDevice;
    gckKERNEL kernel;

    pdev = Platform->device;

    galDevice = platform_get_drvdata(pdev);

    kernel = galDevice->kernels[gcvCORE_MAJOR];

    if (kernel != gcvNULL)
    {
        force_contiguous_lowmem_shrink(kernel);
    }
    else
    {
        gcmkPRINT("%s(%d) can't find kernel! ", __FUNCTION__, __LINE__);
    }

    return gcvSTATUS_OK;
}
#endif

#if gcdENABLE_FSCALE_VAL_ADJUST
#ifdef CONFIG_DEVICE_THERMAL
static int thermal_hot_pm_notify(struct notifier_block *nb, unsigned long event,
       void *dummy)
{
    static gctUINT orgFscale, minFscale, maxFscale;
    static gctBOOL bAlreadyTooHot = gcvFALSE;
    gckHARDWARE hardware;
    gckGALDEVICE galDevice;

    galDevice = platform_get_drvdata(pdevice);
    if (!galDevice)
    {
        /* GPU is not ready, so it is meaningless to change GPU freq. */
        return NOTIFY_OK;
    }

    if (!galDevice->kernels[gcvCORE_MAJOR])
    {
        return NOTIFY_OK;
    }

    hardware = galDevice->kernels[gcvCORE_MAJOR]->hardware;

    if (!hardware)
    {
        return NOTIFY_OK;
    }

    if (event && !bAlreadyTooHot) {
        gckHARDWARE_GetFscaleValue(hardware,&orgFscale,&minFscale, &maxFscale);
        gckHARDWARE_SetFscaleValue(hardware, minFscale);
        bAlreadyTooHot = gcvTRUE;
        gckOS_Print("System is too hot. GPU3D will work at %d/64 clock.\n", minFscale);
    } else if (!event && bAlreadyTooHot) {
        gckHARDWARE_SetFscaleValue(hardware, orgFscale);
        gckOS_Print("Hot alarm is canceled. GPU3D clock will return to %d/64\n", orgFscale);
        bAlreadyTooHot = gcvFALSE;
    }
    return NOTIFY_OK;
}

static struct notifier_block thermal_hot_pm_notifier = {
    .notifier_call = thermal_hot_pm_notify,
    };
#endif

static ssize_t show_gpu3DMinClock(struct device_driver *dev, char *buf)
{
    gctUINT currentf,minf,maxf;
    gckGALDEVICE galDevice;

    galDevice = platform_get_drvdata(pdevice);
    if(galDevice->kernels[gcvCORE_MAJOR])
    {
         gckHARDWARE_GetFscaleValue(galDevice->kernels[gcvCORE_MAJOR]->hardware,
            &currentf, &minf, &maxf);
    }
    snprintf(buf, PAGE_SIZE, "%d\n", minf);
    return strlen(buf);
}

static ssize_t update_gpu3DMinClock(struct device_driver *dev, const char *buf, size_t count)
{

    gctINT fields;
    gctUINT MinFscaleValue;
    gckGALDEVICE galDevice;

    galDevice = platform_get_drvdata(pdevice);
    if(galDevice->kernels[gcvCORE_MAJOR])
    {
         fields = sscanf(buf, "%d", &MinFscaleValue);
         if (fields < 1)
             return -EINVAL;

         gckHARDWARE_SetMinFscaleValue(galDevice->kernels[gcvCORE_MAJOR]->hardware,MinFscaleValue);
    }

    return count;
}

static DRIVER_ATTR(gpu3DMinClock, S_IRUGO | S_IWUSR, show_gpu3DMinClock, update_gpu3DMinClock);
#endif




static const struct of_device_id mxs_gpu_dt_ids[] = {
    { .compatible = "fsl,imx6q-gpu", },
    { .compatible = "fsl,imx-gpu-subsystem", },
    {/* sentinel */}
};
MODULE_DEVICE_TABLE(of, mxs_gpu_dt_ids);


struct contiguous_mem_pool {
    struct dma_attrs attrs;
    dma_addr_t phys;
    void *virt;
    size_t size;
};

struct imx_priv {
    /* Clock management.*/
    struct clk         *clk_3d_core;
    struct clk         *clk_3d_shader;
    struct clk         *clk_3d_axi;
    struct clk         *clk_2d_core;
    struct clk         *clk_2d_axi;
    struct clk         *clk_vg_axi;

       /*Run time pm*/
       struct device           *pmdev;
    struct contiguous_mem_pool *pool;
    struct reset_control *rstc[gcdMAX_GPU_COUNT];
};

static struct imx_priv imxPriv;

gceSTATUS
gckPLATFORM_AdjustParam(
    IN gckPLATFORM Platform,
    OUT gcsMODULE_PARAMETERS *Args
    )
{
     struct resource* res;
     struct platform_device* pdev = Platform->device;

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phys_baseaddr");
    if (res)
        Args->baseAddress = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "irq_3d");
    if (res)
        Args->irqLine = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iobase_3d");
    if (res)
    {
        Args->registerMemBase = res->start;
        Args->registerMemSize = res->end - res->start + 1;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "irq_2d");
    if (res)
        Args->irqLine2D = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iobase_2d");
    if (res)
    {
        Args->registerMemBase2D = res->start;
        Args->registerMemSize2D = res->end - res->start + 1;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "irq_vg");
    if (res)
        Args->irqLineVG = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iobase_vg");
    if (res)
    {
        Args->registerMemBaseVG = res->start;
        Args->registerMemSizeVG = res->end - res->start + 1;
    }

#if gcdENABLE_FSCALE_VAL_ADJUST
    Args->gpu3DMinClock = initgpu3DMinClock;
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_AllocPriv(
    IN gckPLATFORM Platform
    )
{
    Platform->priv = &imxPriv;

#ifdef CONFIG_GPU_LOW_MEMORY_KILLER
    task_free_register(&task_nb);
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_FreePriv(
    IN gckPLATFORM Platform
    )
{
#ifdef CONFIG_GPU_LOW_MEMORY_KILLER
    task_free_unregister(&task_nb);
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_GetPower(
    IN gckPLATFORM Platform
    )
{
    struct device* pdev = &Platform->device->dev;
    struct imx_priv *priv = Platform->priv;
    struct reset_control *rstc;

#ifdef CONFIG_PM
    /*Init runtime pm for gpu*/
    pm_runtime_use_autosuspend(pdev);
    pm_runtime_set_autosuspend_delay(pdev, 200);
    pm_runtime_enable(pdev);
    priv->pmdev = pdev;
#endif


    rstc = devm_reset_control_get(pdev, "gpu3d");
    priv->rstc[gcvCORE_MAJOR] = IS_ERR(rstc) ? NULL : rstc;
    rstc = devm_reset_control_get(pdev, "gpu2d");
    priv->rstc[gcvCORE_2D] = IS_ERR(rstc) ? NULL : rstc;
    rstc = devm_reset_control_get(pdev, "gpuvg");
    priv->rstc[gcvCORE_VG] = IS_ERR(rstc) ? NULL : rstc;

    /*Initialize the clock structure*/
    priv->clk_3d_core = clk_get(pdev, "gpu3d_clk");
    if (!IS_ERR(priv->clk_3d_core)) {
               priv->clk_3d_axi = clk_get(pdev, "gpu3d_axi_clk");
               priv->clk_3d_shader = clk_get(pdev, "gpu3d_shader_clk");
               if (IS_ERR(priv->clk_3d_shader)) {
                   clk_put(priv->clk_3d_core);
                   priv->clk_3d_core = NULL;
                   priv->clk_3d_shader = NULL;
                   gckOS_Print("galcore: clk_get gpu3d_shader_clk failed, disable 3d!\n");
               }
    } else {
        priv->clk_3d_core = NULL;
        gckOS_Print("galcore: clk_get gpu3d_clk failed, disable 3d!\n");
    }

    priv->clk_2d_core = clk_get(pdev, "gpu2d_clk");
    if (IS_ERR(priv->clk_2d_core)) {
        priv->clk_2d_core = NULL;
        gckOS_Print("galcore: clk_get 2d core clock failed, disable 2d/vg!\n");
    } else {
        priv->clk_2d_axi = clk_get(pdev, "gpu2d_axi_clk");
        if (IS_ERR(priv->clk_2d_axi)) {
            priv->clk_2d_axi = NULL;
            gckOS_Print("galcore: clk_get 2d axi clock failed, disable 2d\n");
        }

        priv->clk_vg_axi = clk_get(pdev, "openvg_axi_clk");
        if (IS_ERR(priv->clk_vg_axi)) {
               priv->clk_vg_axi = NULL;
               gckOS_Print("galcore: clk_get vg clock failed, disable vg!\n");
        }
    }


#if gcdENABLE_FSCALE_VAL_ADJUST
    pdevice = Platform->device;
    REG_THERMAL_NOTIFIER(&thermal_hot_pm_notifier);
    {
        int ret = 0;
        ret = driver_create_file(pdevice->dev.driver, &driver_attr_gpu3DMinClock);
        if(ret)
            dev_err(&pdevice->dev, "create gpu3DMinClock attr failed (%d)\n", ret);
    }
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_PutPower(
    IN gckPLATFORM Platform
    )
{
    struct imx_priv *priv = Platform->priv;

    /*Disable clock*/
    if (priv->clk_3d_axi) {
       clk_put(priv->clk_3d_axi);
       priv->clk_3d_axi = NULL;
    }
    if (priv->clk_3d_core) {
       clk_put(priv->clk_3d_core);
       priv->clk_3d_core = NULL;
    }
    if (priv->clk_3d_shader) {
       clk_put(priv->clk_3d_shader);
       priv->clk_3d_shader = NULL;
    }
    if (priv->clk_2d_core) {
       clk_put(priv->clk_2d_core);
       priv->clk_2d_core = NULL;
    }
    if (priv->clk_2d_axi) {
       clk_put(priv->clk_2d_axi);
       priv->clk_2d_axi = NULL;
    }
    if (priv->clk_vg_axi) {
       clk_put(priv->clk_vg_axi);
       priv->clk_vg_axi = NULL;
    }

#ifdef CONFIG_PM
    if(priv->pmdev)
        pm_runtime_disable(priv->pmdev);
#endif

#if gcdENABLE_FSCALE_VAL_ADJUST
    UNREG_THERMAL_NOTIFIER(&thermal_hot_pm_notifier);

    driver_remove_file(pdevice->dev.driver, &driver_attr_gpu3DMinClock);
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_SetPower(
    IN gckPLATFORM Platform,
    IN gceCORE GPU,
    IN gctBOOL Enable
    )
{
    struct imx_priv* priv = Platform->priv;

    if (Enable)
    {
#ifdef CONFIG_PM
	pm_runtime_get_sync(priv->pmdev);
#endif
    }
    else
    {
#ifdef CONFIG_PM
        pm_runtime_put_sync(priv->pmdev);
#endif
    }

    return gcvSTATUS_OK;
}

gceSTATUS
_SetClock(
    IN gckPLATFORM Platform,
    IN gceCORE GPU,
    IN gctBOOL Enable
    )
{
    struct imx_priv* priv = Platform->priv;
    struct clk *clk_3dcore = priv->clk_3d_core;
    struct clk *clk_3dshader = priv->clk_3d_shader;
    struct clk *clk_3d_axi = priv->clk_3d_axi;
    struct clk *clk_2dcore = priv->clk_2d_core;
    struct clk *clk_2d_axi = priv->clk_2d_axi;
    struct clk *clk_vg_axi = priv->clk_vg_axi;


    if (Enable) {
        switch (GPU) {
        case gcvCORE_MAJOR:
            clk_prepare(clk_3dcore);
            clk_enable(clk_3dcore);
            clk_prepare(clk_3dshader);
            clk_enable(clk_3dshader);
            clk_prepare(clk_3d_axi);
            clk_enable(clk_3d_axi);
            break;
        case gcvCORE_2D:
            clk_prepare(clk_2dcore);
            clk_enable(clk_2dcore);
            clk_prepare(clk_2d_axi);
            clk_enable(clk_2d_axi);
            break;
        case gcvCORE_VG:
            clk_prepare(clk_2dcore);
            clk_enable(clk_2dcore);
            clk_prepare(clk_vg_axi);
            clk_enable(clk_vg_axi);
            break;
        default:
            break;
        }
    } else {
        switch (GPU) {
        case gcvCORE_MAJOR:
            clk_disable(clk_3dshader);
            clk_unprepare(clk_3dshader);
            clk_disable(clk_3dcore);
            clk_unprepare(clk_3dcore);
            clk_disable(clk_3d_axi);
            clk_unprepare(clk_3d_axi);
            break;
       case gcvCORE_2D:
            clk_disable(clk_2dcore);
            clk_unprepare(clk_2dcore);
            clk_disable(clk_2d_axi);
            clk_unprepare(clk_2d_axi);
            break;
        case gcvCORE_VG:
            clk_disable(clk_2dcore);
            clk_unprepare(clk_2dcore);
            clk_disable(clk_vg_axi);
            clk_unprepare(clk_vg_axi);
            break;
        default:
            break;
        }
    }

    return gcvSTATUS_OK;
}

#ifdef CONFIG_PM
static int gpu_runtime_suspend(struct device *dev)
{
#ifdef CONFIG_MXC_BUSFREQ
    release_bus_freq(BUS_FREQ_HIGH);
#endif
    return 0;
}

static int gpu_runtime_resume(struct device *dev)
{
#ifdef CONFIG_MXC_BUSFREQ
    request_bus_freq(BUS_FREQ_HIGH);
#endif
    return 0;
}

static struct dev_pm_ops gpu_pm_ops;
#endif

gceSTATUS
_AdjustDriver(
    IN gckPLATFORM Platform
    )
{
    struct platform_driver * driver = Platform->driver;
    driver->driver.of_match_table = mxs_gpu_dt_ids;

    /* Fill local structure with original value. */
    memcpy(&gpu_pm_ops, driver->driver.pm, sizeof(struct dev_pm_ops));

    /* Add runtime PM callback. */
#ifdef CONFIG_PM
    gpu_pm_ops.runtime_suspend = gpu_runtime_suspend;
    gpu_pm_ops.runtime_resume = gpu_runtime_resume;
    gpu_pm_ops.runtime_idle = NULL;

    /* Replace callbacks. */
    driver->driver.pm = &gpu_pm_ops;
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_Reset(
    IN gckPLATFORM Platform,
    gceCORE GPU
    )
{
    struct imx_priv* priv = Platform->priv;
    struct reset_control *rstc = priv->rstc[GPU];
    if (rstc)
        reset_control_reset(rstc);
    return gcvSTATUS_OK;
}

gcsPLATFORM_OPERATIONS platformOperations = {
    .adjustParam  = gckPLATFORM_AdjustParam,
    .allocPriv    = _AllocPriv,
    .freePriv     = _FreePriv,
    .getPower     = _GetPower,
    .putPower     = _PutPower,
    .setPower     = _SetPower,
    .setClock     = _SetClock,
    .adjustDriver = _AdjustDriver,
    .reset        = _Reset,
#ifdef CONFIG_GPU_LOW_MEMORY_KILLER
    .shrinkMemory = _ShrinkMemory,
#endif
};

void
gckPLATFORM_QueryOperations(
    IN gcsPLATFORM_OPERATIONS ** Operations
    )
{
     *Operations = &platformOperations;
}

