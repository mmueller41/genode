/*
 * \brief  Linux emulation environment specific to this driver
 * \author Christian Helmuth
 * \date   2023-05-22
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>


unsigned long __FIXADDR_TOP = 0xfffff000;


#include <linux/uaccess.h>

#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_from_user(void * to, const void __user * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#include <linux/gfp.h>

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return (unsigned long)__alloc_pages(GFP_KERNEL, 0, 0, NULL)->virtual;
}

void * page_frag_alloc_align(struct page_frag_cache *nc,
                             unsigned int fragsz, gfp_t gfp_mask,
                             unsigned int align_mask)
{
	struct page *page;

	if (fragsz > PAGE_SIZE) {
		printk("no support for fragments larger than PAGE_SIZE\n");
		lx_emul_trace_and_stop(__func__);
	}

	page = __alloc_pages(gfp_mask, 0, 0, NULL);

	if (!page)
		return NULL;

	return page->virtual;
}

void page_frag_free(void * addr)
{
	struct page *page = lx_emul_virt_to_page(addr);
	if (!page) {
		printk("BUG %s: page for addr: %p not found\n", __func__, addr);
		lx_emul_backtrace();
	}

	__free_pages(page, 0ul);
}


#include <asm/hardirq.h>

void ack_bad_irq(unsigned int irq)
{
	printk(KERN_CRIT "unexpected IRQ trap at vector %02x\n", irq);
}


#include <linux/pci.h>

void __iomem * pci_ioremap_bar(struct pci_dev * pdev, int bar)
{
	struct resource *res = &pdev->resource[bar];
	return ioremap(res->start, resource_size(res));
}

int pci_read_config_word(const struct pci_dev *dev, int where, u16 *val)
{
	switch (where) {
	case PCI_COMMAND:
		*val = 0x7;
		return 0;

	/*
	 * drivers/net/ethernet/intel/e1000e/ich8lan.c e1000_platform_pm_pch_lpt
	 */
	case 0xa8:
	case 0xaa:
		*val = 0;
		return 0;
	/*
	 * drivers/net/ethernet/intel/e1000e/netdev.c e1000_flush_desc_rings
	 *
	 * In i219, the descriptor rings must be emptied before resetting the HW or
	 * before changing the device state to D3 during runtime (runtime PM).
	 *
	 * Failure to do this will cause the HW to enter a unit hang state which
	 * can only be released by PCI reset on the device
	 */
	case 0xe4:
		/* XXX report no need to flush */
		*val = 0;
		return 0;
	};

	printk("%s: unexpected read at %x\n", __func__, where);
	PCI_SET_ERROR_RESPONSE(val);;
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}

int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val)
{
	printk("%s: unsupported pos=%x\n", __func__, pos);
	PCI_SET_ERROR_RESPONSE(val);;
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


int pcie_capability_write_word(struct pci_dev * dev, int pos, u16 val)
{
	printk("%s: unsupported pos=%x\n", __func__, pos);
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


int pcie_capability_clear_and_set_word_unlocked(struct pci_dev *dev, int pos, u16 clear, u16 set)
{
	printk("%s: unsupported pos=%x\n", __func__, pos);
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


int pcie_capability_clear_and_set_word_locked(struct pci_dev *dev, int pos, u16 clear, u16 set)
{
	return pcie_capability_clear_and_set_word_unlocked(dev, pos, clear, set);
}


int pcie_set_readrq(struct pci_dev * dev, int rq)
{
	printk("%s: unsupported rq=%d\n", __func__, rq);
	return pcibios_err_to_errno(PCIBIOS_FUNC_NOT_SUPPORTED);
}


static unsigned long *_pci_iomap_table;

void __iomem * const * pcim_iomap_table(struct pci_dev * pdev)
{
	unsigned i;

	if (!_pci_iomap_table)
		_pci_iomap_table = kzalloc(sizeof (unsigned long*) * 6, GFP_KERNEL);

	if (!_pci_iomap_table)
		return NULL;

	for (i = 0; i < 6; i++) {
		struct resource *r = &pdev->resource[i];
		unsigned long phys_addr = r->start;
		unsigned long size      = r->end - r->start;

		if (!(r->flags & IORESOURCE_MEM))
			continue;

		if (!phys_addr || !size)
			continue;

		_pci_iomap_table[i] =
			(unsigned long)lx_emul_io_mem_map(phys_addr, size);
	}

	return (void const *)_pci_iomap_table;
}


int pci_select_bars(struct pci_dev *dev, unsigned long flags)
{
	int bars = 0;
	unsigned long const *table;
	unsigned i;

	if (!(flags & IORESOURCE_MEM))
		return 0;

	/* misuse 'pcim_iomap_table()' for querying I/O mem */
	table = (unsigned long const *)pcim_iomap_table(dev);

	for (i = 0; i < 6; i++)
		if (table[i])
			bars |= (1 << i);

	return bars;
}


int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
                                   unsigned int max_vecs, unsigned int flags,
                                   struct irq_affinity *aff_desc)
{
	if ((flags & PCI_IRQ_LEGACY) && min_vecs == 1 && dev->irq)
		return 1;
	return -ENOSPC;
}


int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
                          unsigned int max_vecs, unsigned int flags)
{
	return pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs, flags, NULL);
}


int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (WARN_ON_ONCE(nr > 0))
		return -EINVAL;
	return dev->irq;
}


#include <linux/dma-mapping.h>

void *dmam_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma_handle,
                       gfp_t gfp, unsigned long attrs)
{
	return dma_alloc_attrs(dev, size, dma_handle, gfp, attrs);
}

/*
 * TODO IGB and IXGB wrappers
 */

unsigned int cpumask_local_spread(unsigned int i, int node)
{
	printk("%s: unsupported i=%d node=%d\n", __func__, i, node);
	return 0xff;
}


void ethtool_sprintf(u8 **data, const char *fmt, ...)
{
	printk("%s: unsupported\n", __func__);
}


int rhashtable_init(struct rhashtable *ht,
		    const struct rhashtable_params *params)
{
	printk("%s: unsupported\n", __func__);
	return 0;
}

void *rhashtable_insert_slow(struct rhashtable *ht, const void *key,
			     struct rhash_head *obj)
{
	printk("%s: unsupported\n", __func__);
	return NULL;
}

// struct ndmsg
#include <linux/neighbour.h>

// struct nlattr
#define KBUILD_MODNAME "lx_emul"
#include <linux/netlink.h>

int ndo_dflt_fdb_add(struct ndmsg *ndm,
		     struct nlattr *tb[],
		     struct net_device *dev,
		     const unsigned char *addr, u16 vid,
		     u16 flags)
{
	printk("%s: unsupported\n", __func__);
	return -EINVAL;
}

int ndo_dflt_bridge_getlink(struct sk_buff *skb, u32 pid, u32 seq,
			    struct net_device *dev, u16 mode,
			    u32 flags, u32 mask, int nlflags,
			    u32 filter_mask,
			    int (*vlan_fill)(struct sk_buff *skb,
					     struct net_device *dev,
					     u32 filter_mask))
{
	printk("%s: unsupported\n", __func__);
	return -EMSGSIZE;
}

void __page_frag_cache_drain(struct page *page, unsigned int count)
{
	printk("%s: unsupported page=%p, count=%d\n", __func__, page, count);
}

struct nlattr *nla_find(const struct nlattr *head, int len, int attrtype)
{
	printk("%s: unsupported head=%p, len=%d\n", __func__, head, len);
	return NULL;
}

void *vmalloc_node(unsigned long size, int node)
{
	printk("%s: unsupported size=%d, node=%d\n", __func__, size, node);
	return NULL;
}

// struct flow_match_eth_addrs
#include <net/flow_offload.h>

bool __skb_flow_dissect(const struct net *net,
			const struct sk_buff *skb,
			struct flow_dissector *flow_dissector,
			void *target_container, const void *data,
			__be16 proto, int nhoff, int hlen, unsigned int flags)
{
	printk("%s: unsupported\n", __func__);
	return false;
}

u32 __skb_get_poff(const struct sk_buff *skb, const void *data,
		   const struct flow_keys_basic *keys, int hlen)
{
	printk("%s: unsupported\n", __func__);
	return 0;
}

struct flow_dissector flow_keys_basic_dissector __read_mostly;

void flow_rule_match_basic(const struct flow_rule *rule,
			   struct flow_match_basic *out)
{
	printk("%s: unsupported\n", __func__);
	//FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_BASIC, out);
}

void flow_rule_match_eth_addrs(const struct flow_rule *rule,
			       struct flow_match_eth_addrs *out)
{
	printk("%s: unsupported\n", __func__);
	//FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ETH_ADDRS, out);
}

void flow_rule_match_vlan(const struct flow_rule *rule,
			  struct flow_match_vlan *out)
{
	printk("%s: unsupported\n", __func__);
	//FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_VLAN, out);
}

int flow_block_cb_setup_simple(struct flow_block_offload *f,
			       struct list_head *driver_block_list,
			       flow_setup_cb_t *cb,
			       void *cb_ident, void *cb_priv,
			       bool ingress_only)
{
	printk("%s: unsupported\n", __func__);
	return -EBUSY;
}

// struct i2c_adapter
#include <linux/i2c.h>

int i2c_bit_add_bus(struct i2c_adapter *adap)
{
	printk("%s: unsupported\n", __func__);
	return -ENODEV;
}

void i2c_del_adapter(struct i2c_adapter *adap)
{
	printk("%s: unsupported\n", __func__);
}

s32 i2c_smbus_read_byte_data(const struct i2c_client *client, u8 command)
{
	printk("%s: unsupported\n", __func__);
	return 0;
}

s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command,
			      u8 value)
{
	printk("%s: unsupported\n", __func__);
	return 0;
}


int ipv6_find_hdr(const struct sk_buff *skb, unsigned int *offset,
		  int target, unsigned short *fragoff, int *flags)
{
	printk("%s: unsupported\n", __func__);
	return -ENOENT;
}

bool pci_device_is_present(struct pci_dev *pdev)
{
	printk("%s: unsupported\n", __func__);
	return true;
}

void __iomem *pci_iomap_range(struct pci_dev *dev,
			      int bar,
			      unsigned long offset,
			      unsigned long maxlen)
{
	printk("%s: unsupported\n", __func__);
	return NULL;
}

void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	printk("%s: unsupported\n", __func__);
	return pci_iomap_range(dev, bar, 0, maxlen);
}

void pcie_print_link_status(struct pci_dev *dev)
{
	printk("%s: unsupported\n", __func__);
	//__pcie_print_link_status(dev, true);
}

// struct mdio_if_info
#include <linux/mdio.h>

int mdio45_probe(struct mdio_if_info *mdio, int prtad)
{
	printk("%s: unsupported\n", __func__);
	return -ENODEV;
}

int mdio_mii_ioctl(const struct mdio_if_info *mdio,
		   struct mii_ioctl_data *mii_data, int cmd)
{
	printk("%s: unsupported\n", __func__);
	return -EINVAL;
}

// struct xdp_buff
#include <net/xdp.h>

void xdp_do_flush(void)
{
	printk("%s: unsupported\n", __func__);
	//__dev_flush();
	//__cpu_map_flush();
	//__xsk_map_flush();
}

int xdp_do_redirect(struct net_device *dev, struct xdp_buff *xdp,
		    struct bpf_prog *xdp_prog)
{
	printk("%s: unsupported\n", __func__);
	return -ENOENT;
}

//#error "is this thing on?"
