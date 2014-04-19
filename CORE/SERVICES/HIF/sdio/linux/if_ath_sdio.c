/*
 * Copyright (c) 2010,2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#include <osdep.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include "bmi_msg.h" /* TARGET_TYPE_ */
#include "if_ath_sdio.h"

#ifndef ATH_BUS_PM
#ifdef CONFIG_PM
#define ATH_BUS_PM
#endif /* CONFIG_PM */
#endif /* ATH_BUS_PM */

typedef void * hif_handle_t;
typedef void * hif_softc_t;

extern int hdd_wlan_startup(struct device *dev, void *hif_sc);

struct ath_hif_sdio_softc *sc = NULL;

static A_STATUS
ath_hif_sdio_probe(void *context, void *hif_handle)
{
    int ret = 0;
    struct ol_softc *ol_sc;
    HIF_DEVICE_OS_DEVICE_INFO os_dev_info;
    struct sdio_func *func = NULL;
    u_int32_t target_type;
    ENTER();

    sc = (struct ath_hif_sdio_softc *) A_MALLOC(sizeof(*sc));
    if (!sc) {
        ret = -ENOMEM;
        goto err_alloc;
    }
    A_MEMZERO(sc,sizeof(*sc));


    sc->hif_handle = hif_handle;
    HIFConfigureDevice(hif_handle, HIF_DEVICE_GET_OS_DEVICE, &os_dev_info, sizeof(os_dev_info));

    sc->aps_osdev.device = os_dev_info.pOSDevice;
    sc->aps_osdev.bc.bc_bustype = HAL_BUS_TYPE_SDIO;
    spin_lock_init(&sc->target_lock);

    {
        /*
         * Attach Target register table.  This is needed early on --
         * even before BMI -- since PCI and HIF initialization (and BMI init)
         * directly access Target registers (e.g. CE registers).
         *
         * TBDXXX: targetdef should not be global -- should be stored
         * in per-device struct so that we can support multiple
         * different Target types with a single Host driver.
         * The whole notion of an "hif type" -- (not as in the hif
         * module, but generic "Host Interface Type") is bizarre.
         * At first, one one expect it to be things like SDIO, USB, PCI.
         * But instead, it's an actual platform type. Inexplicably, the
         * values used for HIF platform types are *different* from the
         * values used for Target Types.
         */

#if defined(CONFIG_AR9888_SUPPORT)
        hif_register_tbl_attach(HIF_TYPE_AR9888);
        target_register_tbl_attach(TARGET_TYPE_AR9888);
        target_type = TARGET_TYPE_AR9888;
#elif defined(CONFIG_AR6320_SUPPORT)
        hif_register_tbl_attach(HIF_TYPE_AR6320);
        target_register_tbl_attach(TARGET_TYPE_AR6320);
        target_type = TARGET_TYPE_AR6320;

#endif
    }
    func = ((HIF_DEVICE*)hif_handle)->func;

    ol_sc = A_MALLOC(sizeof(*ol_sc));
    if (!ol_sc){
           ret = -ENOMEM;
           goto err_attach;
    }
    OS_MEMZERO(ol_sc, sizeof(*ol_sc));
    ol_sc->sc_osdev = &sc->aps_osdev;
    ol_sc->hif_sc = (void *)sc;
    sc->ol_sc = ol_sc;
    ol_sc->target_type = target_type;
    ol_sc->enableuartprint = 1;
    ol_sc->enablefwlog = 0;
    ol_sc->enablesinglebinary = FALSE;
    ol_sc->max_no_of_peers = 1;

    ol_sc->hif_hdl = hif_handle;
    init_waitqueue_head(&ol_sc->sc_osdev->event_queue);

    VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_ERROR," calling hdd_wlan_startup...");

    ret = hdd_wlan_startup(&(func->dev), ol_sc);

    if ( ret ) {
        VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_FATAL," hdd_wlan_startup failed");
        goto err_attach1;
    }else{
        VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_INFO," hdd_wlan_startup success!");
    }

    return 0;
err_attach1:
A_FREE(ol_sc);
err_attach:
   A_FREE(sc);
   sc = NULL;

err_alloc:
    return ret;
}

int
ol_ath_sdio_configure(hif_softc_t hif_sc, struct net_device *dev, hif_handle_t *hif_hdl)
{
    struct ath_hif_sdio_softc *sc = (struct ath_hif_sdio_softc *) hif_sc;
    int ret = 0;

    sc->aps_osdev.netdev = dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    SET_MODULE_OWNER(dev);
#endif

    *hif_hdl = sc->hif_handle;

    return ret;
}

static A_STATUS
ath_hif_sdio_remove(void *context, void *hif_handle)
{
    ENTER();

    if (sc && sc->ol_sc){
       A_FREE(sc->ol_sc);
       sc->ol_sc = NULL;
    }
    if (sc) {
        A_FREE(sc);
        sc = NULL;
    }

    EXIT();
    return 0;
}

static A_STATUS
ath_hif_sdio_suspend(void *context)
{
    printk(KERN_INFO "ol_ath_sdio_suspend TODO\n");
    return 0;
}

static A_STATUS
ath_hif_sdio_resume(void *context)
{
    printk(KERN_INFO "ol_ath_sdio_resume ODO\n");
    return 0;
}

static A_STATUS
ath_hif_sdio_power_change(void *context, A_UINT32 config)
{
    printk(KERN_INFO "ol_ath_sdio_power change TODO\n");
    return 0;
}

/*
 * Module glue.
 */
#include "version.h"
static char *version = "HIF (Atheros/multi-bss)";
static char *dev_info = "ath_hif_sdio";

static int init_ath_hif_sdio(void)
{
    static int probed = 0;
    A_STATUS status;
    OSDRV_CALLBACKS osdrvCallbacks;
    ENTER();

    A_MEMZERO(&osdrvCallbacks,sizeof(osdrvCallbacks));
    osdrvCallbacks.deviceInsertedHandler = ath_hif_sdio_probe;
    osdrvCallbacks.deviceRemovedHandler = ath_hif_sdio_remove;
#ifdef CONFIG_PM
    osdrvCallbacks.deviceSuspendHandler = ath_hif_sdio_suspend;
    osdrvCallbacks.deviceResumeHandler = ath_hif_sdio_resume;
    osdrvCallbacks.devicePowerChangeHandler = ath_hif_sdio_power_change;
#endif

    if (probed) {
        return -ENODEV;
    }
    probed++;

    VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_INFO,"%s %d",__func__,__LINE__);
    status = HIFInit(&osdrvCallbacks);
    if(status != A_OK){
       VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_FATAL, "%s HIFInit failed!",__func__);
        return -ENODEV;
    }

    printk(KERN_INFO "%s: %s\n", dev_info, version);

    return 0;
}

int hif_register_driver(void)
{
   int status = 0;
   ENTER();
   status = init_ath_hif_sdio();
   EXIT("status = %d", status);
   return status;

}

void hif_unregister_driver(void)
{
   ENTER();
   HIFShutDownDevice(NULL);
   EXIT();
   return ;
}

void hif_init_adf_ctx(adf_os_device_t adf_dev, void *ol_sc)
{
   struct ol_softc *ol_sc_local = (struct ol_softc *)ol_sc;
   struct ath_hif_sdio_softc *hif_sc = ol_sc_local->hif_sc;
   ENTER();
   adf_dev->drv = &hif_sc->aps_osdev;
   adf_dev->dev = hif_sc->aps_osdev.device;
   ol_sc_local->adf_dev = adf_dev;
   EXIT();
}

void hif_init_pdev_txrx_handle(void *ol_sc, void *txrx_handle)
{
   struct ol_softc *sc = (struct ol_softc *)ol_sc;
   ENTER();
   sc->pdev_txrx_handle = txrx_handle;
}

void hif_disable_isr(void *ol_sc)
{
   ENTER("- dummy function!");
}

void
HIFSetTargetSleep(HIF_DEVICE *hif_device, A_BOOL sleep_ok, A_BOOL wait_for_it)
{
   ENTER("- dummy function!");
}

void
HIFCancelDeferredTargetSleep(HIF_DEVICE *hif_device)
{
    ENTER("- dummy function!");

}

void hif_reset_soc(void *ol_sc)
{
    ENTER("- dummy function!");
}
