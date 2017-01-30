#ifndef _CHANNEL_H
#define _CHANNEL_H
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#define CHAN_ERROR -1
#define CHAN_READABLE 1
#define CHAN_WRITABLE 2

typedef void Channel_Proc(void *clientData, int evmask);
typedef struct Channel Channel;

typedef struct ChannelHandler {
	Channel *chan;
	Channel_Proc *notifyProc;
	void *clientData;
	struct ChannelHandler *next;
	struct ChannelHandler *prev;
	char mask;
	//char busy;
} ChannelHandler;

struct Channel {
	void *implementor;	/* The class which implements the channel interface */
	 ssize_t(*read) (void *impl, void *buf, size_t count);
	 ssize_t(*write) (void *impl, const void *buf, size_t count);
	int (*eof) (void *impl);
	void (*close) (Channel * ch, void *impl);
	void (*addHandler) (ChannelHandler * ch, Channel * chan, void *impl, int mask,
			    Channel_Proc * proc, void *clientData);
	void (*removeHandler) (ChannelHandler * ch, void *impl);
	int busy;
};

static inline void
Channel_Close(Channel * chan)
{
	if (chan->close) {
		if (chan->busy > 0) {
			fprintf(stderr, "Bug: Close Channel while busy\n");
			exit(1);
		}
		chan->busy++;
		chan->close(chan, chan->implementor);
		/* chan->busy--;  May not exist any more */
	}
}

static inline int
Channel_Eof(Channel * chan)
{
	if (chan->eof) {
		int result;
		chan->busy++;
		result = chan->eof(chan->implementor);
		chan->busy--;
		return result;
	} else {
		return 1;	/* If not implemented return always EOF */
	}
}

static inline void
Channel_AddHandler(ChannelHandler * ch, Channel * chan, int mask, Channel_Proc * proc,
		   void *clientData)
{
	if (chan->addHandler) {
		chan->busy++;
		chan->addHandler(ch, chan, chan->implementor, mask, proc, clientData);
		chan->busy--;
	}
}

static inline void
Channel_RemoveHandler(ChannelHandler * ch)
{
	Channel *chan = ch->chan;
	if (chan->removeHandler) {
		chan->busy++;
		chan->removeHandler(ch, ch->chan->implementor);
		chan->busy--;
	}
}

static inline ssize_t
Channel_Read(Channel * chan, void *buf, size_t count)
{
	if (chan->read) {
		ssize_t result;
		chan->busy++;
		result = chan->read(chan->implementor, buf, count);
		chan->busy--;
		return result;
	} else {
		return CHAN_ERROR;
	}
}

static inline ssize_t
Channel_Write(Channel * chan, const void *buf, size_t count)
{
	if (chan->write) {
		ssize_t result;
		chan->busy++;
		result = chan->write(chan->implementor, buf, count);
		chan->busy--;
		return result;
	} else {
		return CHAN_ERROR;
	}
}

#endif
