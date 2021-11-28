/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "mux.h"

#include <memory>
#include <string.h>

void
Channel::Close() {
  mMux->ChannelAskClose(mChanId);
}

bool
Channel::Send(MuxPacket* aPacket) {
  if (!IsValid()) {
    return false;
  }
  assert(aPacket->mChan == mChanId);
  aPacket->mSeq = mSeq++;
  if (aPacket->mSeq < first_seq) {
    aPacket->mSeq = first_seq;
  }
  auto ok = mMux->DoSend(aPacket);
  if (!ok) {
    mValid = false;
  }
  return ok;
}

void
Channel::HandleIncomingPacket(const MuxPacket* aPacket) {
  assert(aPacket->mChan == mChanId);
  assert(aPacket->mSeq == mSeqPeer);
  assert(IsValid());

  assert(aPacket->mCmd != MuxPacket::MuxCmd::Data ||
         (reinterpret_cast<const DataPacket*>(aPacket)->mPayloadSize + sizeof(DataPacket)
          == aPacket->mSize));

  mSeqPeer++;
  mSeqPeer = std::max(mSeqPeer, first_seq);
  if (mListener) {
    mListener->OnReceive(this, aPacket);
  }
}

bool
StreamChannel::Write(unsigned int aSize, const char *aData) {
  auto remain = aSize;
  auto ptr = aData;
  while (remain > 0) {
    auto tocopy = std::min(remain, DataPacket::MaxPayloadSize);
    std::unique_ptr<DataPacket> pkt(DataPacket::Create(tocopy, GetChannelId()));
    memcpy(pkt->mPayload, ptr, tocopy);
    auto ok = Send(pkt.get());
    if (!ok) {
      return false;
    }
    ptr += tocopy;
    remain -= tocopy;
  }
  return true;
}

void
StreamChannel::OnReceive(Channel* aChannel, const MuxPacket* aPacket) {
  assert(aPacket->mCmd == MuxPacket::MuxCmd::Data);
  if (mListener) {
    auto datapkt = reinterpret_cast<const DataPacket*>(aPacket);
    mListener->OnReceive(this, datapkt->mPayloadSize, datapkt->mPayload);
  }
}

void
StreamChannel::OnClose(Channel* aChannel) {
  assert(mListener != nullptr);
  mListener->OnClose(this);
}

void
StreamChannel::OnError(Channel* aChannel, Channel::Error aErr) {
  assert(mListener != nullptr);
  mListener->OnError(this, aErr);
}

Mux::Mux(MuxSide aSide)
  : mSide(aSide)
  , mControlChan(0, 0, this)
  , mNextChanId(4 + (0x1 & (int)aSide)) {
  mControlChan.SetListener(this);
}

unsigned int
Mux::Open(const char* aPath) {
  std::unique_ptr<OpenChanPacket> open(OpenChanPacket::Create(aPath));
  auto ok = mControlChan.Send(open.get());
  if (!ok) {
    return 0;
  }
  auto seq = open->mSeq;        // filled by |Channel::Send()|
  mWaitingPackets[seq] = open.release();
  return seq;
}

void
Mux::OnReceive(Channel* aChannel, const MuxPacket* aPacket) {
  assert(aChannel == &mControlChan);
  switch(aPacket->mCmd) {
  case MuxPacket::MuxCmd::Hello:
    assert(aPacket->mSize == sizeof(MuxPacket));
    break;

  case MuxPacket::MuxCmd::OpenChan:
    {
      assert(mChanListener != nullptr);
      auto request = reinterpret_cast<const OpenChanPacket*>(aPacket);
      assert(OpenChanPacket::SizeOfPathLen(request->mPathLen) == aPacket->mSize);
      assert(request->mPath[request->mPathLen] == 0);
      Channel *channel;
      auto sz = mChanListener->OnCheckDataSize(request->mPath);
      if (sz < 0) {
        auto reply =
          std::make_unique<OpenChanReplyPacket>(request->mSeq,
                                                OpenChanReplyPacket::UnknownError,
                                                0,
                                                0);
        mControlChan.Send(reply.get());
        break;
      }
      if (sz == 0) {
        auto channel_ = new StreamChannel(mNextChanId, this);
        channel = channel_->GetChannel();
      } else {
        channel = new Channel(mNextChanId, sz, this);
      }
      auto chan_id = mNextChanId;
      mChannels[chan_id] = channel;
      mNextChanId += 2;
      while (GetChannel(mNextChanId)) {
        mNextChanId += 2;
      }

      auto reply =
        std::make_unique<OpenChanReplyPacket>(request->mSeq,
                                              OpenChanReplyPacket::Ok,
                                              sz,
                                              chan_id);
      mControlChan.Send(reply.get());
      mChanListener->OnChannelOpened(request->mSeq, 0, channel, request->mPath, false);
      break;
    }

  case MuxPacket::MuxCmd::OpenChanReply:
    {
      assert(mChanListener != nullptr);
      assert(aPacket->mSize == sizeof(OpenChanReplyPacket));

      auto reply = reinterpret_cast<const OpenChanReplyPacket*>(aPacket);
      assert(GetChannel(reply->mDataChan) == nullptr);
      assert((reply->mDataChan & CHANNEL_MASK) != (int)mSide);

      auto pitr = mWaitingPackets.find(reply->mReqSeq);
      assert(pitr != mWaitingPackets.end());

      std::unique_ptr<OpenChanPacket> request(reinterpret_cast<OpenChanPacket*>(pitr->second));
      assert(request->mCmd == MuxPacket::MuxCmd::OpenChan);

      mWaitingPackets.erase(pitr);

      if (reply->mErrno != 0) {
        mChanListener->OnChannelOpened(reply->mReqSeq, reply->mErrno, nullptr, nullptr, true);
        break;
      }

      Channel* channel;
      if (reply->mDataSize == 0) {
        auto channel_ = new StreamChannel(reply->mDataChan, this);
        channel = channel_->GetChannel();
      } else {
        channel = new Channel(reply->mDataChan, reply->mDataSize, this);
      }
      mChannels[reply->mDataChan] = channel;
      mChanListener->OnChannelOpened(reply->mReqSeq, 0, channel, request->mPath, true);
      break;
    }
  }
}

void
Mux::HandleIncomingPacket(const MuxPacket* aPacket) {
  switch (aPacket->mCmd) {
  case MuxPacket::MuxCmd::CloseChan:
    {
      // It can fail silently since both sides can send CloseChan
      // packets much the same time.
      assert(aPacket->mSize == sizeof(MuxPacket));
      DoCloseChannel(aPacket->mChan);
      break;
    }

  case MuxPacket::MuxCmd::Nak:
    {
      assert(aPacket->mSize == sizeof(MuxPacket));
      DoErrorChannel(aPacket->mChan, Channel::Error::UnknownError);
      break;
    }

  default:
    {
      // Route to the associated channel.
      auto channel = GetChannel(aPacket->mChan);
      if (channel == nullptr) {
        SendNak(aPacket->mChan, aPacket->mSeq);
        break;
      }
      if (channel->IsValid()) {
        channel->HandleIncomingPacket(aPacket);
      } else {
        SendNak(aPacket->mChan, aPacket->mSeq);
      }
      break;
    }
  }
}

void
Mux::OnClose(Channel* aChannel) {
}

void
Mux::OnError(Channel* aChannel, PacketListener::Error aErr) {
}

void
Mux::ChannelAskClose(unsigned int aChanId) {
  auto channel = GetChannel(aChanId);
  assert(channel != nullptr);

  if (channel->IsValid()) {
    auto close = std::make_unique<MuxPacket>(sizeof(MuxPacket), aChanId, MuxPacket::MuxCmd::CloseChan);
    channel->Send(close.get());
  }
  
  auto ok = DoCloseChannel(aChanId);
  assert(ok);
}

bool
Mux::DoCloseChannel(unsigned int aChanId) {
  auto chan = GetChannel(aChanId);
  if (chan == nullptr) {
    return false;
  }
  chan->OnClose();
  mChannels.erase(aChanId);
  delete chan;
  return true;
}

bool
Mux::DoErrorChannel(unsigned int aChanId, Channel::Error aErr) {
  auto chan = GetChannel(aChanId);
  if (chan == nullptr) {
    return false;
  }
  chan->OnError((Channel::Error)aErr);
  mChannels.erase(aChanId);
  delete chan;
  return true;
}

bool
Mux::SendNak(unsigned int aChanId, unsigned int aSeq) {
  auto nak = std::make_unique<MuxPacket>(sizeof(MuxPacket), aChanId, MuxPacket::MuxCmd::Nak);
  // The sequence number of a nak packet are the same as the packet
  // received and triggering the nak packet.
  nak->mSeq = aSeq;
  auto ok = DoSend(nak.get());
  return ok;
}

bool
Mux::DoSend(MuxPacket* aPacket) {
  auto r = mTransport->Write(aPacket, aPacket->mSize);
  auto ok = r == aPacket->mSize;
  return ok;
}

bool
Mux::ReceiveRaw(char* aData, unsigned int aSize) {
  auto src = aData;
  auto remain = aSize;
  while (remain) {
    // Full the input buffer.
    auto copy = std::min(remain, MuxPacket::MaxPacketSize - mInputBufSize);
    if (mInputBufSize == 0 && remain >= sizeof(MuxPacket)) {
      // The whole packet header is in |aData|.
      auto packet = reinterpret_cast<MuxPacket*>(src);
      copy = std::min(copy, packet->mSize);
    } else if (mInputBufSize >= sizeof(MuxPacket)) {
      // The whole packet header is in |mInputBuf|.
      auto packet = reinterpret_cast<MuxPacket*>(mInputBuf);
      copy = std::min((int)copy, std::max(0, (int)packet->mSize - (int)mInputBufSize));
    }

    if (copy > 0) {
      memcpy(mInputBuf + mInputBufSize, src, copy);
    }

    remain -= copy;
    mInputBufSize += copy;
    src += copy;

    if (mInputBufSize < sizeof(MuxPacket)) {
      // Content is smaller than smallest packets.
      break;
    }

    auto packet = reinterpret_cast<MuxPacket*>(mInputBuf);
    if (packet->mSize > MuxPacket::MaxPacketSize) {
      // The packet size is too big.
      return false;
    }
    if (packet->mSize < mInputBufSize) {
      // Content is not enough.
      break;
    }

    HandleIncomingPacket(packet);

    // Move the content in the input buffer.
    auto rsz = mInputBufSize - packet->mSize;
    if (rsz > 0) {
      memmove(mInputBuf,
              mInputBuf + packet->mSize,
              rsz);
    }
    mInputBufSize = rsz;
  }
  return remain == 0;
}


#ifdef TEST

#include <list>
#include <functional>
#include <cstdio>

class Mock : public ChannelListener, public Transport {
public:
  struct ResourceInfo {
    int mSize;
    PacketListener* mHandler;
    unsigned int mSeq;
    unsigned int mChanId;
    unsigned int mErr;
    bool mFromPeer;

    ResourceInfo(int aSize, PacketListener* aHandler)
      : mSize(aSize)
      , mHandler(aHandler) {}
    ResourceInfo(ResourceInfo&& aOther)
      : mSize(aOther.mSize)
      , mHandler(aOther.mHandler) {
      aOther.mHandler = nullptr;
    }

    ~ResourceInfo() {
      if (mHandler) {
        delete mHandler;
      }
    }
  };

  void SetPeer(Mock* aPeer) {
    mPeer = aPeer;
  }
  void SetMux(Mux *aMux) {
    mMux = aMux;
  }

  virtual void OnChannelOpened(unsigned int aSeq,
                               unsigned int aErr,
                               Channel* aChannel,
                               const char* aPath,
                               bool aFromPeer) override;
  virtual int OnCheckDataSize(const char* aPath) override;

  virtual int Write(void *aData, int aSize) override;

  void Dispatch();

  void AddResource(int aId, int aSize, PacketListener* aHandler) {
    mResources.emplace(aId, std::forward<ResourceInfo>(ResourceInfo(aSize, aHandler)));
  }
  ResourceInfo* GetResource(int aId) {
    auto itr = mResources.find(aId);
    if (itr == mResources.end()) {
      return nullptr;
    }
    return &itr->second;
  }

private:
  struct Frame {
    unsigned int mSize;
    char mData[];

    Frame(unsigned int aSize) : mSize(aSize) {}

    static Frame* Create(unsigned int aSize) {
      auto buf = new char[sizeof(Frame) + aSize];
      return new(buf) Frame(aSize);
    }
  };

  Mock* mPeer;
  Mux* mMux;
  std::list<Frame*> mIncomings;
  std::map<int, ResourceInfo> mResources;
};

void
Mock::OnChannelOpened(unsigned int aSeq,
                      unsigned int aErr,
                      Channel* aChannel,
                      const char* aPath,
                      bool aFromPeer) {
  auto id = atoi(aPath);
  auto resource = GetResource(id);
  assert(resource != nullptr);
  if (resource) {
    resource->mErr = aErr;
    resource->mFromPeer = aFromPeer;
    if (aErr == 0) {
      aChannel->SetListener(resource->mHandler);
      resource->mSeq = aSeq;
      resource->mChanId = aChannel->GetChannelId();
    }
  }
}

int
Mock::OnCheckDataSize(const char* aPath) {
  printf("Mock::OnCheckDataSize path %s\n", aPath);
  auto id = atoi(aPath);
  auto resource = GetResource(id);
  if (resource) {
    printf("  size %d\n", resource->mSize);
    return resource->mSize;
  }
  return -1;
}

int
Mock::Write(void *aData, int aSize) {
  printf("Mock::Write size %d\n", aSize);
  auto frame = Frame::Create(aSize);
  memcpy(frame->mData, aData, aSize);
  assert(mPeer != nullptr);
  mPeer->mIncomings.push_back(frame);
  return aSize;
}

void
Mock::Dispatch() {
  assert(mMux != nullptr);
  for (auto itr = mIncomings.begin(); itr != mIncomings.end(); ++itr) {
    printf("Mock::Dispatch packet size %d\n", (*itr)->mSize);
    mMux->ReceiveRaw((*itr)->mData, (*itr)->mSize);
    delete *itr;
  }
  mIncomings.clear();
}

class MockPacketListener : public PacketListener {
public:
  typedef std::function<void (const MuxPacket*)> ReceiverType;

  MockPacketListener(ReceiverType&& aReceiver) : mReceiver(std::forward<ReceiverType>(aReceiver)) {}

  virtual void OnReceive(Channel* aChannel, const MuxPacket* aPacket) override {
    printf("MockPacketListener::OnReceiver channel %d, packet size %d\n",
           aChannel->GetChannelId(), aPacket->mSize);
    mReceiver(aPacket);
  }
  virtual void OnClose(Channel* aChannel) override {
    mClosed = true;
  }
  virtual void OnError(Channel* aChannel, PacketListener::Error aErr) override {
    mError = true;
  }

  ReceiverType mReceiver;
  bool mClosed;
  bool mError;
};

void
test_open() {
  int result1 = 0;
  int result2 = 0;
  auto listener1 = new MockPacketListener(MockPacketListener::ReceiverType([&](const MuxPacket* aPacket) {
        assert(aPacket->mCmd == MuxPacket::MuxCmd::Data);
        auto data = reinterpret_cast<const DataPacket*>(aPacket);
        for (int i = 0; i < data->mPayloadSize; i++) {
          result1 += data->mPayload[i];
        }
      }));
  auto listener2 = new MockPacketListener(MockPacketListener::ReceiverType([&](const MuxPacket* aPacket) {
        assert(aPacket->mCmd == MuxPacket::MuxCmd::Data);
        auto data = reinterpret_cast<const DataPacket*>(aPacket);
        for (int i = 0; i < data->mPayloadSize; i++) {
          result2 += data->mPayload[i];
        }
      }));
  Mock mock1;
  Mock mock2;
  mock1.SetPeer(&mock2);
  mock2.SetPeer(&mock1);

  mock1.AddResource(2, 8181, listener1);
  auto resource1 = mock1.GetResource(2);
  mock2.AddResource(2, 18181, listener2);
  auto resource2 = mock2.GetResource(2);

  Mux mux1(Mux::MuxSide::SideServer);
  Mux mux2(Mux::MuxSide::SideClient);
  mux1.SetChanListener(&mock1);
  mux1.SetTransport(&mock1);
  mux2.SetChanListener(&mock2);
  mux2.SetTransport(&mock2);
  mock1.SetMux(&mux1);
  mock2.SetMux(&mux2);

  printf("Open 2\n");
  auto seq_open = mux1.Open("2");
  mock2.Dispatch();
  assert(!resource2->mFromPeer);
  assert(resource2->mSeq == seq_open);
  assert(resource2->mErr == 0);
  mock1.Dispatch();
  assert(resource1->mFromPeer);
  assert(resource1->mSeq == seq_open);
  assert(resource1->mErr == 0);

  assert(resource1->mChanId == resource2->mChanId);

  auto chan_id = resource1->mChanId;
  auto chan1 = mux1.GetChannel(chan_id);

  auto sz = 389;
  printf("Send data %d\n", sz);
  std::unique_ptr<DataPacket> data(DataPacket::Create(sz, chan_id));
  for (int i = 0; i < sz; i++) {
    data->mPayload[i] = 3;
  }
  chan1->Send(data.get());
  assert(result2 == 0);
  mock2.Dispatch();
  assert(result2 == sz * 3);

  auto chan2 = mux2.GetChannel(chan_id);

  sz = 512;
  printf("Send data %d\n", sz);
  data = std::unique_ptr<DataPacket>(DataPacket::Create(sz, chan_id));
  for (int i = 0; i < sz; i++) {
    data->mPayload[i] = 5;
  }
  chan2->Send(data.get());
  assert(result1 == 0);
  mock1.Dispatch();
  assert(result1 == sz * 5);

  assert(!listener1->mClosed);
  assert(!listener2->mClosed);
  printf("Close channel %d\n", chan_id);
  chan1->Close();
  assert(listener1->mClosed);
  assert(!listener2->mClosed);
  mock2.Dispatch();
  assert(listener2->mClosed);

  assert(mux1.GetChannel(chan_id) == nullptr);
  assert(mux2.GetChannel(chan_id) == nullptr);

  assert(listener1->mError == 0);
  assert(listener2->mError == 0);
}

int
main(int argc, const char* argv[]) {
  test_open();
}

#endif
