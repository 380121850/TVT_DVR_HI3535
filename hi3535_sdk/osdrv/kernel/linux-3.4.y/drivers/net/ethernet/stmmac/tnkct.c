/*****************************************************************************
 * This is the driver for the CreVinn TOE-NK-2G TCP Offload Engine.
 * TOE-NK-2G incorporates a Synopsys Ethernet MAC core.
 *
 * Copyright (C) 2011 Emutex Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors: Mark Burkley  <mark@emutex.com>
 *          Kai Ji        <kai@emutex.com>
 *          Dan O'Donovan <dan@emutex.com>
 *
 ******************************************************************************/

/*
 *  TNK connection table manager
 */
#include <linux/tcp.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/workqueue.h>

#include <net/addrconf.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/sock.h>
#include <net/tnkdrv.h>

#include "tnkct.h"
#include "tnktcp.h"
#include "tnkhw.h"
#include "tnkinfo.h"
#include "tnkmem.h"
#include "tnksysctl.h"
#include "stmmac.h"

#define TNK_TTX_DATA_LIMIT ((1 << 24) - 1)

struct tnk_ct_maxretries_work {
	struct work_struct my_work;
	int cindex;
};

static unsigned int tnk_max_connections;

static struct tnkentry *tnk_entry_list;
static struct tnkentry *tnk_free_entry_list;
static struct tnkentry *tnk_used_entry_list;

static int rx_call_skb;
static struct {
	int connection_count;
	int max_retries;
	int discards;
	int ttx_drain_fails;
} ct_proc;

static spinlock_t tnk_ct_lock;

#ifndef TNK_RX_CHANNEL_FLOW_CONTROL
atomic_t tnk_rx_budget;
#endif

#undef TNK_CT_USE_WORKQUEUE
#ifdef TNK_CT_USE_WORKQUEUE
struct workqueue_struct *tnk_ct_workqueue;

static void tnk_ct_work_handler(struct work_struct *data);
DECLARE_WORK(tnk_ct_work, tnk_ct_work_handler);

#else

static void ct_rx_tasklet_function(unsigned long data);
DECLARE_TASKLET(ct_rx_tasklet, ct_rx_tasklet_function, 0);

#endif

static atomic_t tnk_num_active_connections;

MODULE_LICENSE("GPL");

static int tnk_ct_tx_backlog_flush(int cindex, int discard, int limit);

static void ct_list_add(struct tnkentry **list, struct tnkentry *node)
{
	struct tnkentry *last = NULL;

	if (*list)
		for (last = *list; last->next != NULL; last = last->next)
			;

	node->prev = last;
	node->next = NULL;

	if (last)
		last->next = node;
	else
		*list = node;
}

static void ct_list_del(struct tnkentry **list, struct tnkentry *node)
{
	if (!node->prev)
		*list = node->next;
	else
		node->prev->next = node->next;

	if (node->next)
		node->next->prev = node->prev;

	node->next = node->prev = NULL;
}

static void tnk_ct_tx_max_retries_work_handler(struct work_struct *work)
{
	struct tnkentry *e;
	struct sock *sk;
	struct tnk_ct_maxretries_work *my_work =
		(struct tnk_ct_maxretries_work *) work;
	int cindex = my_work->cindex;

	e = &tnk_entry_list[cindex];
	sk = e->sk;

	/*  Remove the connection from the TOE and raise an error */
	if (sk) {
		CONTROL_DBG("%s: TTX max Retries exceeded on connection %d\n",
			    __func__, cindex);
		ct_proc.max_retries++;

		/* when ct_max_retry interrupt happen and is handled,
		* tnk_tcp_sendmsg() may call stream_wait_memory() success
		* and begin to alloc skb to send by TOE, this cause
		* t->entry NULL pointer. */
		lock_sock(sk);
		tnk_tcp_close(sk, 0);

		sk->sk_err = EPIPE;
		sk->sk_error_report(sk);

		tcp_done(sk);
		release_sock(sk);
	}

	kfree(work);
}

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL

int tnk_ct_adv_wnd_update(unsigned cindex)
{
	struct tnkentry *e = &tnk_entry_list[cindex];
	int qlen, new_adv_wnd = 0;
	unsigned long flags;
	int wnd_update_flag = 0;
	struct tcp_sock *tp = tcp_sk(e->sk);
	unsigned int rcv_buf;

	/* when remove the connection tp can be null */
	if (!e->sk)
		goto out;

	spin_lock_irqsave(&e->adv_wnd_lock, flags);

	qlen = atomic_read(&e->rx_q_bytes);
	rcv_buf = e->sk->sk_rcvbuf;

	/* TODO - WIP; need to add a mechanism to completely block
	 * sender if queue is getting too full.  Consider disabling
	 * the connection table entry temporarily to block ACKs
	 *
	 * Setting the window size to 0 isn't good because the
	 * TOE won't respond to the subsequent window probes
	 */
#if 1
	if (qlen >= rcv_buf * 4)
		new_adv_wnd = 0;
	else if (qlen >= rcv_buf * 2)
		new_adv_wnd = rcv_buf / 32;
	else if (qlen >= rcv_buf)
		new_adv_wnd = rcv_buf / 8;
	else if (qlen >= rcv_buf / 2)
		new_adv_wnd = rcv_buf / 4;
	else
		new_adv_wnd = rcv_buf;
#endif
#if 0
	if (qlen >= (3 * tnk_rx_q_limit) / 4)
		new_adv_wnd = 0;
	else if (qlen >= (tnk_rx_q_limit >> 1))
		new_adv_wnd = (tnk_rx_q_limit - qlen) >> 1;
	else
		new_adv_wnd = tnk_rx_q_limit - qlen;
#endif

	if (tp != NULL && tp->rx_opt.rcv_wscale)
		new_adv_wnd >>= tp->rx_opt.rcv_wscale;
	if (new_adv_wnd != e->adv_wnd) {
		if (new_adv_wnd == 0) {
			/* We can't advertise a 0 window, so just disable the
			 * connection temporarily */
			TNK_DBG("%s: disabling cindex %d\n", __func__, cindex);
			tnkhw_txadvwnd_update(cindex, 0);
		} else {
			/* If window was 0, we had disabled the connection so
			 * re-enable it */
			if (e->adv_wnd == 0) {
				TNK_DBG("%s: re-enabling cindex %d\n", __func__,
					cindex);
				wnd_update_flag = 1;
				if (tp != NULL)
					tp->rcv_wnd = new_adv_wnd;
			}

			tnkhw_txadvwnd_update(cindex, new_adv_wnd);
		}
		e->adv_wnd = new_adv_wnd;
	}

	spin_unlock_irqrestore(&e->adv_wnd_lock, flags);
out:
	return wnd_update_flag;
}

#else

static int tnk_rx_stalled;

#endif

static atomic_t tasklet_busy;

#ifdef TNK_CT_USE_WORKQUEUE
static void tnk_ct_work_handler(struct work_struct *data)
#else
static void ct_rx_tasklet_function(unsigned long data)
#endif
{
	struct tnkentry *e;
	int rx_limit = tnk_poll_limit;
	int num_tx_connections;
	int tx_limit_per_conn;
	int reschedule = 0;
	int work_done = 0;

	TNK_DBG("%s scheduled\n", __func__);

	/*  TODO - Remove the following debug code later */
	if (atomic_add_return(1, &tasklet_busy) != 1)
		pr_warn("%s: WARNING - reentrancy detected\n", __func__);

	/*  Reclaim completed TTX buffers from the TOE
	 *  This also clears the DMA_TTX_ACK_INT status */
	if (tnkhw_tx_reclaim()) {
		/*  tnkhw_tx_reclaim() non-zero, poll all active channels */
		spin_lock(&tnk_ct_lock);
		for (e = tnk_used_entry_list; e != NULL; e = e->next)
			tnkhw_tx_channel_reclaim(e->index, 0);
		spin_unlock(&tnk_ct_lock);
	}
#ifndef TNK_RX_CHANNEL_FLOW_CONTROL
	/*  Service the TOE Receive queue (up to a limited budget) */
	rx_limit = atomic_read(&tnk_rx_budget);
	if (rx_limit > tnk_poll_limit)
		rx_limit = tnk_poll_limit;

	if (rx_limit > 0) {
		tnk_rx_stalled = 0;
#endif

		/*  Process new RX buffers, if any */
		work_done = tnkhw_rx(rx_limit);
		TNK_DBG("%s work done %d of %d\n", __func__, work_done,
			rx_limit);

		if (work_done < rx_limit) {
			if (tnkhw_dma_has_work())
				reschedule = 1;
			else
				tnkhw_dma_interrupt_enable();
		} else {
			reschedule = 1;
		}
#ifndef TNK_RX_CHANNEL_FLOW_CONTROL
	} else {
		tnk_rx_stalled = 1;
	}
#endif

	/*  Limit the number of Tx buffers to shift per tasklet iteration */
	tx_limit_per_conn = tnk_poll_limit;
	num_tx_connections = atomic_read(&tnk_num_active_connections);
	if (num_tx_connections > 0)
		tx_limit_per_conn /= num_tx_connections;
	if (tx_limit_per_conn < 1) {
		/*  Shift at least 1 buffer for each connection */
		tx_limit_per_conn = 1;
	}

	spin_lock(&tnk_ct_lock);
	for (e = tnk_used_entry_list; e != NULL; e = e->next) {
#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
		/*  Check if advertised window should be updated after Rx
		 *  processing */
		tnk_ct_adv_wnd_update(e->index);
#endif
		/*  Submit new buffers queued by the application */
		if (!skb_queue_empty(&e->transmit_queue))
			tnk_ct_tx_backlog_flush(e->index, 0, tx_limit_per_conn);
	}
	spin_unlock(&tnk_ct_lock);

	if (reschedule) {
#ifdef TNK_CT_USE_WORKQUEUE
		queue_work(tnk_ct_workqueue, &tnk_ct_work);
#else
		tasklet_schedule(&ct_rx_tasklet);
#endif
	}

	/*  TODO - Remove the following debug code later */
	atomic_dec(&tasklet_busy);
}

static void tnk_trim_head(struct sk_buff *skb, int len)
{
	skb->data += len;
	skb->len -= len;
	TCP_SKB_CB(skb)->seq += len;
	skb->truesize	     -= len;
}

#if 0
static int tnk_out_of_order_enough_space(struct tnkentry *e, int len)
{
	int qlen;

	qlen = atomic_read(&e->rx_q_bytes);
	if ((qlen + len) < (tnk_rx_q_limit*2 - 1460))
		return 1;

	return 0;
}
#endif
static void tnk_data_queue_ofo(struct tnkentry *e, struct sk_buff *skb)
{
	struct sk_buff *skb1;
	u32 seq, end_seq;

#if 0

	if (!tnk_out_of_order_enough_space(e, skb->len)) {
		/* TODO: should increment a counter */
		atomic_sub(skb->len, &e->rx_q_bytes);
		__kfree_skb(skb);
		goto out;
	}
#endif

	skb1 = skb_peek_tail(&e->out_of_order_queue);
	if (!skb1) {
		__skb_queue_head(&e->out_of_order_queue, skb);
		goto out;
	}

	seq = TCP_SKB_CB(skb)->seq;
	end_seq = TCP_SKB_CB(skb)->end_seq;
	/* Find place to insert this segment. */
	while (1) {
		if (!after(TCP_SKB_CB(skb1)->seq, seq))
			break;
		if (skb_queue_is_first(&e->out_of_order_queue, skb1)) {
			skb1 = NULL;
			break;
		}
		skb1 = skb_queue_prev(&e->out_of_order_queue, skb1);
	}

	/* Do skb overlap to previous one? */
	if (skb1 && before(seq, TCP_SKB_CB(skb1)->end_seq)) {
		if (!after(end_seq, TCP_SKB_CB(skb1)->end_seq)) {
			/* All the bits are present. Drop. */
			/* atomic_sub(skb->len, &e->rx_q_bytes); */
			atomic_sub(DMA_BUFFER_SIZE, &e->rx_q_bytes);
			tnk_mem_read_dequeued(skb);
			__kfree_skb(skb);
			skb = NULL;
			goto out;
		}
		if (after(seq, TCP_SKB_CB(skb1)->seq)) {
			/* Partial overlap. */
		} else {
			if (skb_queue_is_first(&e->out_of_order_queue,
					       skb1))
				skb1 = NULL;
			else
				skb1 = skb_queue_prev(
					&e->out_of_order_queue,
					skb1);
		}
	}
	if (!skb1)
		__skb_queue_head(&e->out_of_order_queue, skb);
	else
		__skb_queue_after(&e->out_of_order_queue, skb1, skb);

	/* And clean segments covered by new one as whole. */
	while (!skb_queue_is_last(&e->out_of_order_queue, skb)) {
		skb1 = skb_queue_next(&e->out_of_order_queue, skb);

		if (!after(end_seq, TCP_SKB_CB(skb1)->seq))
			break;
		if (before(end_seq, TCP_SKB_CB(skb1)->end_seq))
			break;
		__skb_unlink(skb1, &e->out_of_order_queue);
		/* atomic_sub(skb1->len, &e->rx_q_bytes); */
		atomic_sub(DMA_BUFFER_SIZE, &e->rx_q_bytes);
		tnk_mem_read_dequeued(skb1);
		__kfree_skb(skb1);
	}
out:
	return;
}
static void tnk_ct_full_pkt_process(unsigned cindex, struct sk_buff *skb)
{
	struct tnkentry *e = &tnk_entry_list[cindex];
	struct sock *sk = NULL;
	struct net_device *ndev = NULL;
	struct tcp_sock *tcp = NULL;
	struct tnkcb *cb;
	unsigned int flags = TCP_SKB_CB(skb)->header.tcb.flag;
#ifdef TNK_BONDING
	int bonding_flag = 0;
	int bonding_dev = 0;
#endif
	sk = e->sk;
	cb = &(TCP_SKB_CB(skb)->header.tcb);
	tcp = tcp_sk(sk);
	ndev = sk->sk_tnkinfo.dev;
#ifdef TNK_BONDING
	bonding_flag = is_bonding_enable();
	if (bonding_flag) {
		bonding_dev = curr_bonding_dev();
		ndev = stmmac_device_list[bonding_dev];
	}
#endif
	BUG_ON(skb->len < ETH_HLEN);
	skb->protocol = eth_type_trans(skb, ndev);

	if (flags & TNKHW_CB_FLAG_FIN) {
#if 0
		if (sk->sk_tnkinfo.finflag != TNK_TYPE_FIN)
			tnkhw_update_fin_seqnum(cindex);
#endif
		cb->type = TNK_TYPE_FIN;
		TCP_SKB_CB(skb)->tcp_flags |= TCPHDR_FIN;
		sk->sk_tnkinfo.finflag = TNK_TYPE_FIN;
	} else if (flags & TNKHW_CB_FLAG_RST) {
		cb->type = TNK_TYPE_RST;
		TCP_SKB_CB(skb)->tcp_flags |= TCPHDR_RST;
		TCP_SKB_CB(skb)->seq = tcp->rcv_nxt;
		e->rst_received = 1;
		wake_up_interruptible(&e->waitqueue);
		sk->sk_tnkinfo.rstflag = TNK_TYPE_RST;
	} else if (flags & (TNKHW_CB_FLAG_SYN | TNKHW_CB_FLAG_ACK)) {
		pr_warn("%s WARN recevied full packet that is SYN_ACK\n",
				__func__);
		pr_warn("TCP_SKB_CB(skb)->seq = 0x%x, skb->end_seq = 0x%x\n",
				TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq);
	} else {
		pr_warn("%s WARN recevied full packet that isn't FIN or RST\n",
				__func__);
		pr_warn("flags = %x, tcp->rcv_nxt =0x%x, tcp->snd_una = 0x%x",
				flags, tcp->rcv_nxt, tcp->snd_una);
		pr_warn("tcp->copied_seq = 0x%x\n", tcp->copied_seq);
		pr_warn("TCP_SKB_CB(skb)->seq = 0x%x, skb->end_seq = 0x%x\n",
			TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq);
	}

	/* This packet will go slow-path up through the kernel stack
	* later, and it will increment tcp->rcv_nxt and friends so
	* don't do it here */
	spin_unlock(&e->list_lock);
	netif_receive_skb(skb);
	spin_lock(&e->list_lock);
}
static void tnk_ct_rx_callback(struct sk_buff *skb, unsigned cindex,
			       unsigned long flags, unsigned short urgent_ptr)
{
	/* [DOD 09-May-2011] This is the sequence needed to pass it up the stack
	 *
	 * 1. lookup connection in table
	 * 2. get sk pointer, and set in skb header
	 * 3. get net device pointer, and set in skb header
	 * 4. update stats for connection entry, net device, and socket
	 * 5. Add to socket recieve queue, e.g.:
	 *    skb_queue_tail(&sk->sk_receive_queue, skb);
	 * 6. Wake up the socket users to let them know there's data to read
	 *    sk->sk_data_ready(sk, 0);
	 */
	struct tnkentry *e = &tnk_entry_list[cindex];
	struct sock *sk = NULL;
	struct net_device *ndev = NULL;
	struct tcp_sock *tcp = NULL;
	struct tnkcb *cb;
	struct sk_buff *skb1;
	bool   full_pkt_recved = 0;

	BUG_ON(skb == NULL);

	cb = &(TCP_SKB_CB(skb)->header.tcb);
	TCP_SKB_CB(skb)->tcp_flags = 0;
	/*  Lock to protect against possibility of interrupt
	 *  by tnk_ct_remove() */
	spin_lock(&e->list_lock);
	sk = e->sk;

	smp_rmb();
	if (unlikely(sk == NULL || e->sk_rcv_disable)) {
		TNK_DBG("%s: socket pointer for connection %d is NULL,",
				__func__, cindex);
		TNK_DBG("dropping packet\n");
		goto drop;
	}

	tcp = tcp_sk(sk);
	ndev = sk->sk_tnkinfo.dev;

	if (unlikely(!ndev)) {
		pr_err("%s: net_device pointer for connection %d is NULL,",
				__func__, cindex);
		pr_err("dropping packet\n");
				goto drop;
	}

	skb->dev = ndev;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->sk = sk;

	ndev->stats.rx_packets++;
	ndev->stats.rx_bytes += skb->len;

	TNK_DBG("%s rx %d bytes on ch %d\n",
			__func__, skb->len, cindex);
	tnk_mem_read_queued(skb);
	rx_call_skb++;

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
	/* atomic_add(skb->len, &e->rx_q_bytes); */
	atomic_add(DMA_BUFFER_SIZE, &e->rx_q_bytes);
#else
	atomic_dec(&tnk_rx_budget);
#endif
	/* TODO - skb->len won't include stripped-off transport/ip/ll headers
	 * so is slightly misleading for stats (unless full packet flag is
	 * set below). Should we add the estimated header lengths?
	 */

	if (flags)
		TNK_DBG("%s: some flags are set: 0x%X\n",
				__func__, flags);

	if (TCP_SKB_CB(skb)->seq == tcp->rcv_nxt) {
		if (!(cb->flag&TNKHW_CB_FLAG_FULL_PKT)) {
			/*fast path*/
			unsigned long flags;

			cb->type = TNK_TYPE_DATA;
			spin_lock_irqsave(&e->receive_queue.lock, flags);
			__skb_queue_tail(&e->receive_queue, skb);
			tcp->rcv_nxt += skb->len;
			tcp->rcv_wup = tcp->rcv_nxt - 1;
			spin_unlock_irqrestore(&e->receive_queue.lock, flags);
		} else {
			tnk_mem_read_dequeued(skb);
			tnk_ct_full_pkt_process(cindex, skb);
			full_pkt_recved = 1;
			/* atomic_sub(skb1->len, &e->rx_q_bytes); */
			atomic_sub(DMA_BUFFER_SIZE, &e->rx_q_bytes);
		}
	} else {
		cb->type = TNK_TYPE_DATA;
		tnk_data_queue_ofo(e, skb);
	}

	while ((skb1 = skb_peek(&e->out_of_order_queue)) != NULL) {
		if (after(TCP_SKB_CB(skb1)->seq, tcp->rcv_nxt))
			break;

		if (!after(TCP_SKB_CB(skb1)->end_seq, tcp->rcv_nxt)) {
			__skb_unlink(skb1, &e->out_of_order_queue);
			/* atomic_sub(skb1->len, &e->rx_q_bytes); */
			atomic_sub(DMA_BUFFER_SIZE, &e->rx_q_bytes);
			tnk_mem_read_dequeued(skb1);
			__kfree_skb(skb1);
			continue;
		}

		__skb_unlink(skb1, &e->out_of_order_queue);

		if (!(TCP_SKB_CB(skb1)->header.tcb.flag
					& TNKHW_CB_FLAG_FULL_PKT)){
			unsigned long flags;

			tnk_trim_head(skb1,
					tcp->rcv_nxt - TCP_SKB_CB(skb1)->seq);
			/* atomic_sub(tcp->rcv_nxt - TCP_SKB_CB(skb1)->seq,
					&e->rx_q_bytes); */
			spin_lock_irqsave(&e->receive_queue.lock, flags);
			__skb_queue_tail(&e->receive_queue, skb1);
			tcp->rcv_nxt = TCP_SKB_CB(skb1)->end_seq;
			tcp->rcv_wup = tcp->rcv_nxt - 1;
			spin_unlock_irqrestore(&e->receive_queue.lock, flags);
		} else {
			tnk_mem_read_dequeued(skb1);
			tnk_ct_full_pkt_process(cindex, skb1);
			full_pkt_recved = 1;
			/* atomic_sub(skb1->len, &e->rx_q_bytes); */
			atomic_sub(DMA_BUFFER_SIZE, &e->rx_q_bytes);
			WARN_ON(!skb_queue_empty(&e->out_of_order_queue));
		}
	}
	tnkhw_update_rx_seqnum(cindex, tcp->rcv_nxt);

	spin_unlock(&e->list_lock);

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
	tnk_ct_adv_wnd_update(cindex);
#endif

	mb();
	/*  The following function should not be called with irqs disabled */
	if (!skb_queue_empty(&e->receive_queue) && !full_pkt_recved)
		sk->sk_data_ready(sk, 0);
	return;

drop:
	spin_unlock(&e->list_lock);
	tnk_free_skb(skb, SKB_FROM_RECV);
}

static void tnk_ct_poll_wakeup_callback(void)
{
	tnkhw_dma_interrupt_disable();
#ifdef TNK_CT_USE_WORKQUEUE
	queue_work(tnk_ct_workqueue, &tnk_ct_work);
#else
	tasklet_schedule(&ct_rx_tasklet);
#endif
}

static void tnk_ct_txfree_callback(unsigned cindex, unsigned acked_bytes)
{
	struct tnkentry *e = &tnk_entry_list[cindex];

	spin_lock(&e->list_lock);
	if (likely(e->sk)) {
		struct tcp_sock *tcp = tcp_sk(e->sk);
		struct tnkhw_connection conn;

		/* tcp->snd_una += acked_bytes; */
		/* TODO: update tcp->snd_una by adding acked_bytes is
		   not a safety way. Becasue tx_reclaim() update
		   tcp->snd_una, FIN packet goto stack update
		   tcp->snd_una also. This cause tcp->snd_una advance
		   tcp->snd_nxt one skb len. tnk_ct_remove() print
		   "Clsing conn with -6 unacked bytes". */
		tnkhw_connection_get(cindex, &conn);
		tcp->snd_una = conn.rx_ack_num;

		if (jiffies >= e->next_rtt_poll) {
			/*  Poll for a new RTT sample */
			int rtt_sample = tnkhw_rtt_sample(cindex);

			if (rtt_sample >= 0) {
				int new_rto_ms;
				unsigned long rtt_sample_tmp;

				/*  Update the SRTT value and RTO for this
				 *  socket */
				rtt_sample_tmp = msecs_to_jiffies(rtt_sample);
				rcu_read_lock();
				tcp_valid_rtt_meas(e->sk, rtt_sample_tmp);
				rcu_read_unlock();
				/*  Convert socket RTO value from ticks to
				 *  milliseconds */
				new_rto_ms =
				    inet_csk(e->sk)->icsk_rto * 1000 / HZ;

				/*  Update the connection table if the timeout
				 *  has changed */
				if (e->txtimeout != new_rto_ms) {
					tnkhw_txtimeout_update(cindex,
							       new_rto_ms);
					e->txtimeout = new_rto_ms;
				}

				/*  Schedule the next poll */
				e->next_rtt_poll =
				    jiffies + tnk_tcp_rtt_sample_interval;
			}
		}
	}
	spin_unlock(&e->list_lock);
}

static void tnk_ct_tx_max_retries_callback(unsigned cindex)
{
	struct tnkentry *e = &tnk_entry_list[cindex];

	/* disable connection to prevent continuous interrupts. */
	tnkhw_connection_disable(cindex);

	if (e && !test_and_set_bit(0, &e->max_retries_exceeded)) {
		struct tnk_ct_maxretries_work *my_work;
		my_work = kmalloc(sizeof(struct tnk_ct_maxretries_work),
				  GFP_ATOMIC);
		if (my_work) { /*  Freed in work handler */
			INIT_WORK((struct work_struct *)my_work,
				  tnk_ct_tx_max_retries_work_handler);
			my_work->cindex = cindex;
			schedule_work((struct work_struct *)my_work);
		} else {
			pr_err("%s: Failed to allocate memory for deferred",
					__func__);
			pr_err(" work item\n");
		}
	}
}

void tnk_ct_tx_backlog_enqueue(int cindex, struct sk_buff *skb, bool flush)
{
	struct tnkentry *e = &tnk_entry_list[cindex];
	unsigned long flags;

	spin_lock_irqsave(&e->tx_q_lock, flags);
	skb_queue_tail(&e->transmit_queue, skb);
	tnk_mem_write_queued(skb);

	if (e->sk) {
		e->sk->sk_wmem_queued += skb->truesize;
		sk_mem_charge(e->sk, skb->truesize);
	}
	spin_unlock_irqrestore(&e->tx_q_lock, flags);

	if (flush) {
#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
		/*  Check if advertised window should be updated after Rx
		 *  processing */
		tnk_ct_adv_wnd_update(e->index);
#endif
		tnk_ct_tx_backlog_flush(e->index, 0,
				skb_queue_len(&e->transmit_queue));
	}
}

void tnk_ct_tx_backlog_flush_all(int cindex)
{
	struct tnkentry *e = &tnk_entry_list[cindex];

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
	/*  Check if advertised window should be updated after Rx
	 *  processing */
	tnk_ct_adv_wnd_update(e->index);
#endif
	tnk_ct_tx_backlog_flush(e->index, 0,
			skb_queue_len(&e->transmit_queue));
}

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
int tnk_ct_rx_release(int cindex, int len)
{
	struct tnkentry *e = &tnk_entry_list[cindex];

	atomic_sub(len, &e->rx_q_bytes);
	return tnk_ct_adv_wnd_update(cindex);
}
#else
void tnk_ct_rx_release(int count)
{
	/*  If tnk rx is stalled, reschedule the tasklet if we're now above 1/4
	 *  watermark again */
	if ((atomic_add_return(count, &tnk_rx_budget) >= TNK_RX_SW_Q_SIZE / 4)
	    && tnk_rx_stalled) {
#ifdef TNK_CT_USE_WORKQUEUE
		queue_work(tnk_ct_workqueue, &tnk_ct_work);
#else
		tasklet_schedule(&ct_rx_tasklet);
#endif
	}
}
#endif

/*  TODO remove only for debug
 */
void tnk_ct_proc(struct seq_file *s)
{
	seq_printf(s, "\n\ntnkct:\n");
#ifndef TNK_RX_CHANNEL_FLOW_CONTROL
	seq_printf(s, "tnk_rx_budget=%d:\n", atomic_read(&tnk_rx_budget));
#endif
	seq_printf(s, "connection count=%d:\n", ct_proc.connection_count);
	seq_printf(s, "max retries=%d:\n", ct_proc.max_retries);
	seq_printf(s, "discards=%d:\n", ct_proc.discards);
	seq_printf(s, "ttx_drain_fails=%d:\n", ct_proc.ttx_drain_fails);
	seq_printf(s, "tcp_memory_pressure=%d:\n", tcp_memory_pressure);
	seq_printf(s, "tcp_memory_allocated=%d:\n",
		   atomic_read(&tcp_memory_allocated));

#ifdef TNK_CT_USE_WORKQUEUE
	queue_work(tnk_ct_workqueue, &tnk_ct_work);
#else
	tasklet_schedule(&ct_rx_tasklet);
#endif
}

/*  Flush transmit queue */
static int tnk_ct_tx_backlog_flush(int cindex, int discard, int limit)
{
	int err = 0;
	struct tnkentry *e = &tnk_entry_list[cindex];
	struct sock *sk = NULL;
	struct sk_buff *skb;
	unsigned long flags;

	sk = e->sk;

	/*  A limit of -1 effectively means no limit */
	while (limit) {
		TNK_DBG("%s dequeue\n", __func__);

		spin_lock_irqsave(&e->tx_q_lock, flags);

		skb = skb_peek(&e->transmit_queue);
		if (skb != NULL) {
			TNK_DBG("%s data in flight, requeueing skb %p",
					__func__, skb, skb->len);
			TNK_DBG("%d bytes through tnk\n", skb->len);

			if (likely(skb->len > 0) && !discard)
				err = tnkhw_tx_send(skb, cindex);
			else
				err = 0;

			if (err != -EAGAIN) {
				skb_unlink(skb, &e->transmit_queue);
				tnk_mem_write_dequeued(skb);

				if (err != 0)
					discard = 1;

				/* It is essential that we update sk accounting
				 * details, even when discarding skbs, because
				 * they must reflect that what has gone into
				 * the queue has come out again
				 */
				if (sk) {
					sock_set_flag(sk, SOCK_QUEUE_SHRUNK);
					sk->sk_wmem_queued -= skb->truesize;
					sk_mem_uncharge(e->sk, skb->truesize);

					if (!discard) {
						struct net_device *ndev = NULL;
						ndev = sk->sk_tnkinfo.dev;
						ndev->trans_start = jiffies;
						ndev->stats.tx_packets++;
						ndev->stats.tx_bytes +=
							skb->len;
						tcp_sk(sk)->snd_nxt +=
							skb->len;
					}
				}
			}

			limit--;
		}

		if (skb_queue_empty(&e->transmit_queue)) {
			/*  See tnk_ct_stop() */
			wake_up_interruptible(&e->waitqueue);
		}

		spin_unlock_irqrestore(&e->tx_q_lock, flags);

		if (skb == NULL || err == -EAGAIN)
			break;

		if (unlikely(discard)) {
			TNK_DBG("%s send FAIL, or discard flag set!\n",
				__func__);
			ct_proc.discards++;
			tnk_free_skb(skb, SKB_FROM_SEND);

			/* If we got an error, bail out, but if we are just
			 * discarding for shutdown then keep iteration until
			 * queue is empty
			 */
			if (err)
				break;
			else
				continue;
		}

		mb();
		/*  Notify the application that some space has freed up */
		if (sk && sock_flag(sk, SOCK_QUEUE_SHRUNK)) {
			sock_reset_flag(sk, SOCK_QUEUE_SHRUNK);
			if (sk->sk_socket)
				sk->sk_write_space(sk);
		}
	}

	return err;
}

/* This function will be called on any IPv4 address change event */
static int tnk_ct_inetaddr_event(struct notifier_block *this,
				 unsigned long event, void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct net_device *ndev = NULL;
	struct stmmac_priv *priv = NULL;
	int i;

	if (unlikely(!ifa || !ifa->ifa_dev || !ifa->ifa_dev->dev))
		return NOTIFY_DONE;

	/* Check if this is for one of our STMMAC devices
	 * TODO - put a token in the ndev struct to identify as STMMAC?
	 */
#ifdef TNK_BONDING
	if (0 != strncmp(ifa->ifa_dev->dev->name,
				"bond0",
				strlen(ifa->ifa_dev->dev->name))) {
#endif
		for (i = 0; i < TNK_NUM_GMACS; i++) {
			if (stmmac_device_list[i] == ifa->ifa_dev->dev) {
				ndev = ifa->ifa_dev->dev;
				break;
			}
		}
		if (unlikely(!ndev))
			return NOTIFY_DONE;

		priv = netdev_priv(ndev);

#ifdef TNK_BONDING
	}
#endif

	switch (event) {
	case NETDEV_UP:
			/*  Configure the TOE with new IP address */
#ifdef TNK_BONDING
			if (0 == strncmp(ifa->ifa_dev->dev->name,
					"bond0",
					strlen(ifa->ifa_dev->dev->name))) {
				tnkhw_ipv4address_set
					((uint8_t *) &ifa->ifa_address, 0);
				tnkhw_ipv4address_set
					((uint8_t *) &ifa->ifa_address, 1);
			} else
#endif
				tnkhw_ipv4address_set
					((uint8_t *) &ifa->ifa_address,
					 priv->id);
			break;
	case NETDEV_DOWN:
			/*  Configure the TOE with NULL IP address */
#ifdef TNK_BONDING
			if (0 == strncmp(ifa->ifa_dev->dev->name,
					"bond0",
					strlen(ifa->ifa_dev->dev->name))) {
				tnkhw_ipv4address_rst
					((uint8_t *) &ifa->ifa_address, 0);
				tnkhw_ipv4address_rst
					((uint8_t *) &ifa->ifa_address, 1);
			} else
#ifndef TNK_BONJOUR
				tnkhw_ipv4address_rst
					((uint8_t *) &ifa->ifa_address,
					 priv->id);
#endif
#endif

#ifdef TNK_BONJOUR
			tnkhw_ipv4address_rst((uint8_t *) &ifa->ifa_address,
					priv->id);
#else
			if (ifa->ifa_dev && ifa->ifa_dev->ifa_list == NULL) {
				/*  Last address was deleted
				    from this interface */
				tnkhw_ipv4address_set(NULL, priv->id);
			} else {
				void *addr =
					&ifa->ifa_dev->ifa_list->ifa_address;
				/*  Use the address at the head of the list */
				tnkhw_ipv4address_set((uint8_t *)addr,
						priv->id);
			}
#endif
			break;
	}
	return NOTIFY_DONE;
}

struct notifier_block tnk_ct_inetaddr_notifier = {
	.notifier_call = tnk_ct_inetaddr_event,
};

#ifdef CONFIG_IPV6
/* This function will be called on any IPv6 address change event */
static int tnk_ct_inet6addr_event(struct notifier_block *this,
				  unsigned long event, void *ptr)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *ndev = NULL;
	struct stmmac_priv *priv = NULL;
	int i;

	if (unlikely(!ifa || !ifa->idev || !ifa->idev->dev))
		return NOTIFY_DONE;

	/* Check if this is for one of our STMMAC devices
	 * TODO - put a token in the ndev struct to identify as STMMAC?
	 */
#ifdef TNK_BONDING
	if (0 != strncmp(ifa->idev->dev->name, "bond0",
				strlen(ifa->idev->dev->name))) {
#endif
		for (i = 0; i < TNK_NUM_GMACS; i++) {
			if (stmmac_device_list[i] == ifa->idev->dev) {
				ndev = ifa->idev->dev;
				break;
			}
		}
		if (unlikely(!ndev))
			return NOTIFY_DONE;

		priv = netdev_priv(ndev);
#ifdef TNK_BONDING
	}
#endif
	switch (event) {
	case NETDEV_UP:
		/*  Configure the TOE with new IP address */
#ifdef TNK_BONDING
		if (0 == strncmp(ifa->idev->dev->name,
				"bond0", strlen(ifa->idev->dev->name))) {
			tnkhw_ipv6address_set(ifa->addr.s6_addr, 0);
			tnkhw_ipv6address_set(ifa->addr.s6_addr, 1);
		} else
#endif
			tnkhw_ipv6address_set(ifa->addr.s6_addr, priv->id);
		break;
	case NETDEV_DOWN:
		/*  Configure the TOE with NULL IP address */
#ifdef TNK_BONDING
		if (0 == strncmp(ifa->idev->dev->name,
				"bond0", strlen(ifa->idev->dev->name))) {
			tnkhw_ipv6address_rst(ifa->addr.s6_addr, 0);
			tnkhw_ipv6address_rst(ifa->addr.s6_addr, 1);
		} else
#ifndef TNK_BONJOUR
			tnkhw_ipv6address_rst(ifa->addr.s6_addr,
					priv->id);
#endif
#endif
#ifdef TNK_BONJOUR
		tnkhw_ipv6address_rst(ifa->addr.s6_addr,
				priv->id);
#else /*TNK_BONJOUR*/
		if (ifa->idev && list_empty(&ifa->idev->addr_list)) {
			/*  Last address was deleted from this interface */
			tnkhw_ipv6address_set(NULL, priv->id);
		} else {
			/*  Use the address at the head of the list */
			struct inet6_ifaddr *ifa_next =
				list_first_entry(&ifa->idev->addr_list,
						struct inet6_ifaddr,
						if_list);
			if (ifa_next)
				tnkhw_ipv6address_set(ifa_next->addr.s6_addr,
						priv->id);
		}
#endif
		break;
	}
	return NOTIFY_DONE;
}

struct notifier_block tnk_ct_inet6addr_notifier = {
	.notifier_call = tnk_ct_inet6addr_event,
};
#endif

int tnk_ct_init(unsigned int max_connections)
{
	int i, err;
	tnk_max_connections = max_connections;

	spin_lock_init(&tnk_ct_lock);

	tnk_entry_list =
	    kmalloc(max_connections * sizeof(struct tnkentry), GFP_KERNEL);
	if (!tnk_entry_list) {
		pr_err("%s: Failed to allocate memory for connection table\n",
		       __func__);
		return -ENOMEM;
	}

	tnk_free_entry_list = tnk_used_entry_list = NULL;
	atomic_set(&tnk_num_active_connections, 0);

	TNK_DBG("%s building connections linked list\n", __func__);
	for (i = 2; i < max_connections; i++) {
		struct tnkentry *e = &tnk_entry_list[i];

		e->index = i;
		e->sk = NULL;
		skb_queue_head_init(&e->receive_queue);
		skb_queue_head_init(&e->transmit_queue);
		skb_queue_head_init(&e->out_of_order_queue);
		ct_list_add(&tnk_free_entry_list, e);
	}

#ifndef TNK_RX_CHANNEL_FLOW_CONTROL
	atomic_set(&tnk_rx_budget, TNK_RX_SW_Q_SIZE);
#endif
	atomic_set(&tasklet_busy, 0);

	err = register_inetaddr_notifier(&tnk_ct_inetaddr_notifier);
	if (err) {
		pr_err("%s: Failed to register inetaddr notifier\n", __func__);
		return err;
	}
#ifdef CONFIG_IPV6
	err = register_inet6addr_notifier(&tnk_ct_inet6addr_notifier);
	if (err) {
		pr_err("%s: Failed to register inet6addr notifier\n", __func__);
		return err;
	}
#endif

#ifdef TNK_CT_USE_WORKQUEUE
	tnk_ct_workqueue = create_singlethread_workqueue("tnk");
	if (!tnk_ct_workqueue) {
		pr_err("%s: Failed to create tnk workqueue\n", __func__);
		return -1;
	}
#endif

	TNK_DBG("%s starting dma\n", __func__);
	return tnkhw_dma_start(tnk_ct_poll_wakeup_callback,
			       tnk_ct_rx_callback,
			       tnk_ct_txfree_callback,
			       tnk_ct_tx_max_retries_callback);
}

struct sock *tnk_ct_get_sock(int index)
{
	struct tnkentry *e = &tnk_entry_list[index];
	return e->sk;
}

void tnk_ct_close_active_connections(int graceful)
{
	struct tnkentry *e;

	while ((e = tnk_used_entry_list)) {
		struct sock *sk = e->sk;
		if (sk)
			tnk_tcp_close(sk, graceful);
	}
}

void tnk_ct_shutdown(void)
{
	unregister_inetaddr_notifier(&tnk_ct_inetaddr_notifier);
#ifdef CONFIG_IPV6
	unregister_inet6addr_notifier(&tnk_ct_inet6addr_notifier);
#endif
	flush_scheduled_work();
	tnk_ct_close_active_connections(0);
	tnkhw_dma_stop();
#ifdef TNK_CT_USE_WORKQUEUE
	destroy_workqueue(tnk_ct_workqueue);
#else
	tasklet_kill(&ct_rx_tasklet);
#endif

	kfree(tnk_entry_list);
}

void tnk_ct_prepare(struct sk_buff *skb, struct tnkinfo *tnkinfo,
		    struct net_device *dev, int gmac_id)
{
	/*  TNK_DBG ("%s tnkinfo=%p\n", __func__, tnkinfo); */
	tnkinfo->dev = dev;

	if (skb_mac_header_was_set(skb)) {
		/* Copy the first 6 bytes of the MAC header, which contain the
		 * dst MAC address.
		 * Note that this assumes an outgoing skb (transmit path)
		 */
		memcpy(tnkinfo->rmac, skb_mac_header(skb),
		       sizeof(tnkinfo->rmac));

		TNK_DBG("remote mac %02X-%02X-%02X-%02X-%02X-%02X\n",
			tnkinfo->rmac[0],
			tnkinfo->rmac[1],
			tnkinfo->rmac[2],
			tnkinfo->rmac[3], tnkinfo->rmac[4], tnkinfo->rmac[5]);
	} else {
		pr_err("%s: MAC header not set in outgoing skb\n", __func__);
	}
}

struct tnkentry *tnk_get_free_entry(struct sock *sk)
{
	struct tnkentry *e;

	spin_lock(&tnk_ct_lock);
	e = tnk_free_entry_list;
	if (e)
		ct_list_del(&tnk_free_entry_list, e);
	spin_unlock(&tnk_ct_lock);

	return e;
}

void tnk_ct_probe_timer(unsigned long data)
{
	struct tnkhw_connection con;
	struct tnkentry *e = (struct tnkentry *)data;
	struct tcp_sock *tcp;
	unsigned int diff = -1;
	unsigned int window;


	if (unlikely(!e->sk))
		return;
	tcp = tcp_sk(e->sk);

	tnkhw_connection_get(e->index, &con);
	/* zero window and not the the first timer */
	window = con.rx_adv_wnd >> con.adv_wnd_scale;
	if (window < 1460 && e->last_ack && e->last_snd_nxt) {
		diff = con.next_rx_seq_num - e->last_ack;
		diff += con.next_tx_seq_num - e->last_snd_nxt;
	}
	e->last_ack = con.next_rx_seq_num;
	e->last_snd_nxt = con.next_tx_seq_num;

	if (!diff)
		tnk_send_probe(e->sk, con.next_tx_seq_num, con.next_rx_seq_num);

	mod_timer(&e->zero_window_probe_timer, jiffies + tnk_tcp_persist_time);
}


struct tnkentry *tnk_ct_create(struct sock *sk, struct tnkentry *e)
{
	struct tnkinfo *tnkinfo = &sk->sk_tnkinfo;
	struct inet_sock *inet = inet_sk(sk);
	struct tcp_sock *tcp;
	struct tnkhw_connection conn;
#if defined(TNK_BONDING) || defined(TNK_BONJOUR)
	int  local_ip_pos = 0;
#endif

	TNK_DBG("%s BEEN CALLED !\n", __func__);
	e->max_retries_exceeded = 0;
	e->sk = sk;
	e->info = "";
	e->sk_rcv_disable = 0;
	tcp = tcp_sk(e->sk);
	tnkinfo->finflag = 0;
	tnkinfo->rstflag = 0;
	tnkinfo->howto_destroy = TNK_DESTROY_CLOSE;

	/*  Tell TCP not to generate any more keep alive frames
	 */
	tcp_set_keepalive(sk, 0);

	memset(&conn, 0, sizeof(conn));

	conn.r_macaddr_u16 = (tnkinfo->rmac[0] << 8) | tnkinfo->rmac[1];
	conn.r_macaddr_l32 =
	    (tnkinfo->rmac[2] << 24) |
	    (tnkinfo->rmac[3] << 16) |
	    (tnkinfo->rmac[4] << 8) | (tnkinfo->rmac[5]);

	conn.l_port = ntohs(inet->inet_sport);
	conn.r_port = ntohs(inet->inet_dport);

	/* When we have IPv6 enabled, incoming IPv4 connections
	 * still have sk_family set to AF_INET6 because the listener
	 * is IPv6
	 *
	 * So we use the GSO type instead to figure out the real
	 * because this is one of the few fields in the sk struct
	 * which is set differently for IPv4 vs IPv6
	 */
#ifdef CONFIG_IPV6
	if (sk->sk_gso_type == SKB_GSO_TCPV6) {
		struct ipv6_pinfo *inet6 = inet6_sk(sk);

		conn.flag_ipv6 = 1;
		conn.ttl = inet6->hop_limit;
		conn.tos = inet6->tclass;
		conn.r_ipv6_flow_label = inet6->flow_label;

		conn.r_ipv6_addr_127_96 = ntohl(inet6->daddr.s6_addr32[0]);
		conn.r_ipv6_addr_95_64 = ntohl(inet6->daddr.s6_addr32[1]);
		conn.r_ipv6_addr_63_32 = ntohl(inet6->daddr.s6_addr32[2]);
		conn.r_ipv6_addr_31_0 = ntohl(inet6->daddr.s6_addr32[3]);
#if defined(TNK_BONDING) || defined(TNK_BONJOUR)
		uint32_t ipv6_addr[4] = {0};
		int i;

		for (i = 0; i < 4; i++) {
			ipv6_addr[i] = ntohl(inet6->saddr.s6_addr32[i]);
			TNKBD_DBG("%s ipv6_addr[%d] = %x\n",
					__func__, i, ipv6_addr[i]);
		}
#ifdef TNK_BONDING
		sync_ipv6_pos_to_logic(ipv6_addr);
#endif
		/* Get local ipv6 addr ,let logic konw */
		local_ip_pos = select_ipv6_pos_to_logic
			(sk->sk_tnkinfo.gmac_id, ipv6_addr);

		/* One TOE connection to a 2 bits ip position */
		if (local_ip_pos >= 0)
			e->local_ip_pos = local_ip_pos;
#endif
	} else
#endif
	{
		conn.flag_ipv6 = 0;
		conn.ttl = inet->uc_ttl;
		conn.tos = inet->tos;

		conn.r_ipaddr = ntohl(inet->inet_daddr);
#ifdef TNK_BONDING
		sync_ipv4_pos_to_logic(ntohl(inet->inet_rcv_saddr));
#endif
#if defined(TNK_BONDING) || defined(TNK_BONJOUR)
		/* Get local ip addr ,let logic konw */
		local_ip_pos = select_ipv4_pos_to_logic
			(sk->sk_tnkinfo.gmac_id, ntohl(inet->inet_rcv_saddr));
		TNKBD_DBG("%s ipv4_addr = %x,gmac_id = %d local_ip_pos = %d\n",
				__func__, ntohl(inet->inet_rcv_saddr),
				sk->sk_tnkinfo.gmac_id, local_ip_pos);

		/* One TOE connection to a 2 bits ip position */
		if (local_ip_pos >= 0)
			e->local_ip_pos = local_ip_pos;
#endif
	}

	/*  Assume all data is unacknowledged when setting up
	 *  a TOE connection, so set sequence number to snd_una
	 *  even if data has already been sent.
	 */
	TNK_DBG("%s write_seq=0x%08X\n", __func__, tcp->write_seq);
	TNK_DBG("%s una=      0x%08X (%d)\n", __func__, tcp->snd_una,
		tcp->snd_una - tcp->write_seq);
	TNK_DBG("%s snd=      0x%08X (%d)\n", __func__, tcp->snd_nxt,
		tcp->snd_nxt - tcp->write_seq);
	TNK_DBG("%s rcv=      0x%08X\n", __func__, tcp->rcv_nxt);

	TNK_DBG("%s transmit queue size: %d\n", __func__,
		skb_queue_len(&e->transmit_queue));
	WARN(!skb_queue_empty(&e->transmit_queue),
		"e->index=%d, e->transmit_queue len=%d\n",
		e->index, skb_queue_len(&e->transmit_queue));
	TNK_DBG("%s sk write queue size: %d\n", __func__,
		skb_queue_len(&e->sk->sk_write_queue));

	tcp->snd_nxt = tcp->snd_una;	/*  Resend unack'd data via TOE */
	conn.retry_seq_num = tcp->snd_una;
	conn.next_rx_seq_num = tcp->rcv_nxt;
	conn.next_tx_seq_num = tcp->snd_nxt;

	/*intial value for new reno */
	conn.word4_flag = 0;
	conn.word4_count = 0;
	conn.word5_recoverseqnum = tcp->snd_nxt;

	/*initail value for out of order by sw */
	conn.word31_updated_flag = 0;
	conn.word30_updated_tx_ack_num = 0;

	e->initial_seq = tcp->rcv_nxt;
	e->initial_ack = tcp->snd_nxt;

	conn.sampled_ack_num = conn.rx_ack_num = tcp->snd_una;

	conn.tx_mss = min_t(int, max_mss, tcp_current_mss(sk));

	conn.adv_wnd_scale = tcp->rx_opt.snd_wscale;
	conn.rx_adv_wnd = tcp->rcv_wnd;
	conn.tx_adv_wnd = tcp->snd_wnd;
	conn.cong_wnd = tcp->snd_cwnd * conn.tx_mss;
	/*  Initial congestion window must not be smaller than MSS */
	if (conn.cong_wnd < conn.tx_mss)
		conn.cong_wnd = conn.tx_mss;

	conn.flag_mac_sel = tnkinfo->gmac_id;
	/* don't enable connection when create ct table  */
	conn.flag_conn_enable = 0;
	conn.flag_full_pkt = 0;

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
	/*  Manage the advertised window in software */
	conn.flag_use_local_wnd = 0;
	conn.flag_use_local_scl = 0;
	conn.tx_adv_wnd = e->adv_wnd = sk->sk_rcvbuf;
	atomic_set(&e->rx_q_bytes, 0);
#else
	/*  Let the TOE take care of managing the advertised window */
	conn.flag_use_local_wnd = 1;
	conn.flag_use_local_scl = 1;
#endif

	/* TODO - check if this is correct
	 * Here's my condensed understanding of it:
	 *  (i) if TCP_NAGLE_PUSH is set, disable nagle
	 *  (ii) if TCP_NAGLE_CORK is set, enable nagle
	 *  (iii) if tcp->nonagle is 0, enable nagle
	 *  (iv) otherwise, disable nagle
	 */

	/* TODO - if this is changed later using setsockopt(),
	 * we won't see it...
	 */
#if 0
	conn.flag_disable_nagle = (tcp->nonagle & TCP_NAGLE_PUSH) ||
	    !((tcp->nonagle & TCP_NAGLE_CORK) || !tcp->nonagle) ||
	    (tcp->nonagle == TCP_NAGLE_OFF);
#else
	conn.flag_disable_nagle = 1;
#endif

	TNK_DBG("%s: Nagle algorithm is %s for connection %d\n",
		conn.flag_disable_nagle ? "DISABLED" : "ENABLED", e->index);

	/*  TODO populate rent of conn fields here */
	conn.dup_ack_cnt = 0;
	conn.timer_tx_retry = 0;
	conn.retry_count = 0;

	conn.timer_timestamp = 0;
	conn.timer_active = 0;
	conn.timer_seq_num = 0;
	conn.timer_rx_fast_retry = 0;
	conn.en_rtt_calc = 1;
	conn.ss_threshold = tcp->snd_ssthresh;
	conn.tx_timestamp = 0;
	conn.timeout_value = inet_csk(sk)->icsk_rto * 1000 / HZ;
	conn.timer_rtt = 0;
	conn.timer_rtt_valid = 0;
	conn.last_retry_seq_num = 0;

	/* Set the next RTT poll to happen as soon as possible
	 * (i.e. when we get our first ACK) */
	e->next_rtt_poll = jiffies;
	e->txtimeout = conn.timeout_value;

	e->rst_received = 0;
	init_waitqueue_head(&e->waitqueue);

	spin_lock_init(&e->list_lock);
	spin_lock_init(&e->tx_q_lock);
	spin_lock_init(&e->adv_wnd_lock);

	/* setup zero window probe timer */
	e->last_ack = 0;
	e->last_snd_nxt = 0;
	setup_timer(&e->zero_window_probe_timer, tnk_ct_probe_timer,
			(unsigned long)e);
	mod_timer(&e->zero_window_probe_timer, jiffies + tnk_tcp_persist_time);

	/*  Now that the entry struct is initialised, add it to the list */
	spin_lock(&tnk_ct_lock);
	ct_list_add(&tnk_used_entry_list, e);
	ct_proc.connection_count++;
	atomic_inc(&tnk_num_active_connections);
	spin_unlock(&tnk_ct_lock);

	/*  Everything's ready, tell the TOE to accelerate the connection */
#if defined(TNK_BONDING) || defined(TNK_BONJOUR)
	tnkhw_connection_add(e->index, &conn, e->local_ip_pos);
#else
	tnkhw_connection_add(e->index, &conn);
#endif

	TNK_DBG("%s Added connection %d for sk %p\n", __func__, e->index, sk);

	return e;
}

void tnk_ct_enable(unsigned int cindex)
{
	tnkhw_connection_enable(cindex);
}

/*  This function removes an entry from the hardware table to prevent
 *  any more data being handled by hardware.  It does not add the entry
 *  to the free list in case any frames have been received but not yet
 *  processed by the socket layer.
 */
int tnk_ct_stop(struct tnkentry *e, int tx_done_timeout)
{
	int err = 0;

	int unacked_bytes;
	int tx_done_timeout_init = tx_done_timeout;

	TNK_DBG("%s write_seq= %x\n", __func__, tcp_sk(e->sk)->write_seq);
	TNK_DBG("%s snd_una=   %x (%d)\n", __func__, tcp_sk(e->sk)->snd_una,
		tcp_sk(e->sk)->snd_una - tcp_sk(e->sk)->write_seq);
	TNK_DBG("%s snd_nxt=   %x (%d)\n", __func__, tcp_sk(e->sk)->snd_nxt,
		tcp_sk(e->sk)->snd_nxt - tcp_sk(e->sk)->write_seq);
	TNK_DBG("%s rcv_nxt=   %x\n", __func__, tcp_sk(e->sk)->rcv_nxt);

	TNK_DBG("%s transmit queue size: %d\n", __func__,
		skb_queue_len(&e->transmit_queue));
	TNK_DBG("%s sk write queue size: %d\n", __func__,
		skb_queue_len(&e->sk->sk_write_queue));

	/*  Need to flush the transmit queue first if non-empty */
	while (!skb_queue_empty(&e->transmit_queue) &&
	       tx_done_timeout > 0 && !e->rst_received) {
		DEFINE_WAIT(wait);

#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
		/*  Check if advertised window should be updated after Rx
		 *  processing */
		tnk_ct_adv_wnd_update(e->index);
#endif
		tnk_ct_tx_backlog_flush(e->index, 0,
				skb_queue_len(&e->transmit_queue));

		prepare_to_wait(&e->waitqueue, &wait, TASK_INTERRUPTIBLE);
		if (!skb_queue_empty(&e->transmit_queue) && !e->rst_received)
			tx_done_timeout = schedule_timeout(tx_done_timeout);
		finish_wait(&e->waitqueue, &wait);
	}

	unacked_bytes = tnkhw_tx_done_wait(e->index, tx_done_timeout);

	if (unacked_bytes) {
		TNK_DBG("%s: Failed to drain TTX queue", __func__);
		TNK_DBG(" for connection %d", e->index);
		TNK_DBG("(%d bytes unacknowledged/discarded)\n",
				unacked_bytes);
		/* TODO - get the kernel to send a reset?
		 * Alternatively, claw the unack'd data back from the TOE
		 * descriptor ring and transfer to the socket write queue
		 * for the kernel to deal with
		 */
		err = -ETIMEDOUT;
		ct_proc.ttx_drain_fails++;
	}

	/*  We may need to clean up our transmit queue */
	/*     Even if "unacked_bytes" is zero, we still need to
	 * clean up our transmit queue because transmit queue
	 * maybe not empty when connection is closed not gracefully.
	 *     This can be tested when we are still sending data,
	 * we receive a RST packet.
	 *     If transmit_queue is not empty when close, kernel will
	 * WARNING: stream.c  sk->sk_wmem_queued, sk->sk_forward_alloc */
	if (!skb_queue_empty(&e->transmit_queue))
		tnk_ct_tx_backlog_flush(e->index, 1, -1);

	WARN(!skb_queue_empty(&e->transmit_queue),
		"sk=0x%p, e->index=%d, e->transmit_queue len=%d, "
		"unacked_bytes=%d, tx_done_timeout=%d\n",
		e->sk, e->index, skb_queue_len(&e->transmit_queue),
		unacked_bytes, tx_done_timeout_init);

	TNK_DBG("%s stopping entry %d (sk %p)\n", __func__, e->index, e->sk);

#if 0
	struct tcp_sock *tcp = tcp_sk(e->sk);
	tnkhw_conn_stats_t conn_stats;
	TNK_DBG("%s write_seq=0x%08X\n", __func__, tcp->write_seq);
	TNK_DBG("%s una=      0x%08X (%d)\n", __func__, tcp->snd_una,
		tcp->snd_una - tcp->write_seq);
	TNK_DBG("%s snd=      0x%08X (%d)\n", __func__, tcp->snd_nxt,
		tcp->snd_nxt - tcp->write_seq);
	TNK_DBG("%s rcv=      0x%08X\n", __func__, tcp->rcv_nxt);

	TNK_DBG("Final state of connection %d in TOE:\n", e->index);
	tnkhw_connection_dump(e->index);

	tnkhw_conn_stats_get(e->index, &conn_stats);
	TNK_DBG("Tx Connection Stats:\n"
		"\ttx_desc_sent: %u\n"
		"\ttx_desc_ackd: %u\n"
		"\ttx_bytes_sent: %u\n"
		"\ttx_bytes_ackd: %u\n"
		"\ttx_acked_count: %u\n"
		"\ttx_retry_count: %u\n",
		conn_stats.tx_desc_sent,
		conn_stats.tx_desc_ackd,
		conn_stats.tx_bytes_sent,
		conn_stats.tx_bytes_ackd,
		conn_stats.tx_acked_count, conn_stats.tx_retry_count);

	TNK_DBG("%s transmit queue size: %d\n", __func__,
		skb_queue_len(&e->transmit_queue));
	TNK_DBG("%s sk write queue size: %d\n", __func__,
		skb_queue_len(&e->sk->sk_write_queue));
#endif
	del_timer(&e->zero_window_probe_timer);

	return err;
}

void tnk_ct_remove(struct tnkentry *e)
{
	struct tnkhw_connection conn;
	struct sock *sk = e->sk;
	struct tcp_sock *tcp = tcp_sk(e->sk);

	spin_lock(&tnk_ct_lock);
	ct_list_del(&tnk_used_entry_list, e);
	atomic_dec(&tnk_num_active_connections);
	spin_unlock(&tnk_ct_lock);

	spin_lock(&e->list_lock);

	/*  Get the final snapshot of the connection state */
	tnkhw_connection_get(e->index, &conn);

	/*  Withdraw connection from TOE path */
#if defined(TNK_BONDING) || defined(TNK_BONJOUR)
	tnkhw_connection_remove(e->index, &conn, e->local_ip_pos);
#else
	tnkhw_connection_remove(e->index, &conn);
#endif

	e->sk = NULL;
	smp_wmb();

	spin_unlock(&e->list_lock);

	/* NOTE - we assume that this normally** only gets called on
	 * an outbound FIN, so we assume that the application is no
	 * longer interested in any data we may have in our receive queue
	 *
	 * ** It may also be called in exception cases, such as dropping
	 * a timed-out/errored connection, in which case dropping Rx queue
	 * contents is equally valid
	 *
	 * One situation which highlights the need for this is where FINs
	 * are sent in both directions simultaneously (e.g. FTP does this).
	 * The initial inbound FIN will sit in this queue forever if we
	 * don't dump it here (we could also send it up the stack for
	 * processing, but this may cause additional complications when
	 * done in the context of tcp_send_fin())
	 */
	if (!(skb_queue_empty(&e->receive_queue))) {
		int count = 0;
		int len = 0;
		unsigned long flags;

		/*  Drain the Rx queue if non-empty */
		struct sk_buff *skb;
		struct sk_buff *skb_sk;
		struct tnkcb *cb = NULL;
		spin_lock_irqsave(&e->receive_queue.lock, flags);
		while ((skb = __skb_dequeue(&e->receive_queue))) {
			cb = &TCP_SKB_CB(skb)->header.tcb;
			if ((cb->magic == TNK_MAGIC) &&
				(cb->type == TNK_TYPE_DATA)) {
				tnk_mem_read_dequeued(skb);
			}

			if (sk->sk_tnkinfo.howto_destroy
					== TNK_DESTROY_SHUTDOWN) {
				tnk_tcp_skb_build_tcpheader(skb);

				skb_sk = skb_peek_tail(&sk->sk_receive_queue);
				if (skb_sk && tcp_hdr(skb_sk)->fin) {
					skb_insert(skb_sk, skb,
						&sk->sk_receive_queue);
				} else {
					skb_queue_tail(&sk->sk_receive_queue,
						skb);
				}
			} else {
				tnkhw_skb_recycle(skb);
				len += skb->len;
			}
			count++;
		}
		spin_unlock_irqrestore(&e->receive_queue.lock, flags);

		if (sk->sk_tnkinfo.howto_destroy
				!= TNK_DESTROY_SHUTDOWN) {
#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
			/* tnk_ct_rx_release(e->index, len); */
			tnk_ct_rx_release(e->index, DMA_BUFFER_SIZE * count);
#else
		tnk_ct_rx_release(count);
#endif
		}
	}
	if (!(skb_queue_empty(&e->out_of_order_queue))) {
		struct sk_buff *skb;
		int len = 0;
		int count = 0;

		while ((skb = __skb_dequeue(&e->out_of_order_queue))) {
			tnk_mem_read_dequeued(skb);
			len += skb->len;
			tnkhw_skb_recycle(skb);
			count++;
		}
#ifdef TNK_RX_CHANNEL_FLOW_CONTROL
		/* tnk_ct_rx_release(e->index, len); */
		tnk_ct_rx_release(e->index, DMA_BUFFER_SIZE * count);
#else
		tnk_ct_rx_release(count);
#endif
	}

	if (tcp->snd_una != tcp->snd_nxt) {
		/* We are closing the connection with some unack'd data
		 * We set tcp->snd_una = tcp->snd_nxt to suppress some
		 * warnings that the kernel may emit when closing the socket
		 *
		 * TODO - a more correct, but far more complex solution, for
		 * this case would be to retrieve the unack'd data from the
		 * TTX pipeline and try to re-insert it back onto the
		 * sk_write_queue and let the kernel deal with it.  However, for
		 * now, we choose not to do this because the connection is being
		 * closed anyway so there's little value in having the kernel
		 * wait any longer for the acks
		 */
		TNK_DBG("%s:Closing connection %d with %d bytes",
			__func__, e->index, tcp->snd_nxt - tcp->snd_una);
		TNK_DBG(" of unack'd data\n");
		tcp->snd_una = tcp->snd_nxt;
	}

	spin_lock(&tnk_ct_lock);
	ct_list_add(&tnk_free_entry_list, e);
	spin_unlock(&tnk_ct_lock);
}

/* get the index entry in tnk_entry_list*/
struct tnkentry *tnk_ct_get_entry(int i)
{
	struct tnkentry *e;
	e = &tnk_entry_list[i];

	BUG_ON(i < 2 || i >= tnk_max_connections);

	return e;
}

/*  Get the next entry element from the used list.
 *  Or get the head of the list if NULL
 */

struct tnkentry *tnk_ct_get_next_used(struct tnkentry *e)
{
	if (!e)
		return tnk_used_entry_list;

	return e->next;
}

/* Disable/enable connections associated with a GMAC
 * depending on the PHY link state
 */
void tnk_ct_link_state_update(int gmac_id, int link_up)
{
	struct tnkentry *e;

	spin_lock(&tnk_ct_lock);

	e = tnk_used_entry_list;
	while (e) {
		if (e->sk && (gmac_id == e->sk->sk_tnkinfo.gmac_id)) {
			if (link_up)
				tnkhw_connection_enable(e->index);
			else
				tnkhw_connection_disable(e->index);
		}

		e = e->next;
	}

	spin_unlock(&tnk_ct_lock);
}
