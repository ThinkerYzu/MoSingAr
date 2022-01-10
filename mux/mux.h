/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __MUX_H_
#define __MUX_H_

#include <string.h>
#include <assert.h>
#include <map>
#include <new>

#define MUX_CONTROL_CHANNEL 0

#define SERVER_SIDE_MASK 0x0
#define CLIENT_SIDE_MASK 0x1
#define CHANNEL_MASK 0x1

struct MuxPacket {
  enum MuxCmd {
    Hello,
    Ack,
    Nak,
    // Channel control
    OpenChan,
    OpenChanReply,
    CloseChan,
    // Data payload
    Data,

    LastStdCmd
  };

  static constexpr unsigned int MaxPacketSize = 4096;

  unsigned int mSize;
  unsigned int mChan;
  unsigned int mSeq;            // Will be filled by |Channel::Send()|
                                // for most cases.
  MuxCmd mCmd;

  MuxPacket(unsigned int aSize, unsigned int aChan, MuxCmd aCmd)
    : mSize(aSize)
    , mChan(aChan)
    , mCmd(aCmd) {
    assert(aSize <= MaxPacketSize);
  }
};

struct OpenChanPacket : public MuxPacket {
  unsigned int mPathLen;
  char mPath[];

  static constexpr int MaxPathLen =
    MaxPacketSize - sizeof(MuxPacket) - sizeof(mPathLen);

  OpenChanPacket(unsigned int aPathLen)
    : MuxPacket(SizeOfPathLen(aPathLen),
                0,
                MuxCmd::OpenChan) {
  }

  static unsigned int SizeOfPathLen(unsigned int aLen) {
    return sizeof(OpenChanPacket) + aLen + 1;
  }

  static OpenChanPacket* Create(const char* aPath) {
    auto pathlen = strlen(aPath);
    auto size = SizeOfPathLen(pathlen);
    auto buf = new char[size];
    assert(strlen(aPath) <= MaxPathLen);

    OpenChanPacket* p = new(buf) OpenChanPacket(pathlen);
    p->mPathLen = strlen(aPath);
    strcpy(p->mPath, aPath);
    return p;
  }
};

struct OpenChanReplyPacket : public MuxPacket {
  enum Error {
    Ok = 0,
    UnknownError,
    PermissionDeny
  };
  unsigned int mReqSeq;
  unsigned int mErrno;
  unsigned int mDataSize;
  unsigned int mDataChan;

  OpenChanReplyPacket(unsigned int aReqSeq,
                      unsigned int aErr,
                      unsigned int aDataSize,
                      unsigned int aChan)
    : MuxPacket(sizeof(OpenChanReplyPacket), 0, MuxCmd::OpenChanReply)
    , mReqSeq(aReqSeq)
    , mErrno(aErr)
    , mDataSize(aDataSize)
    , mDataChan(aChan)
  {}
};

struct DataPacket : public MuxPacket {
  unsigned int mPayloadSize;
  char mPayload[];

  static constexpr unsigned int MaxPayloadSize =
    MaxPacketSize - sizeof(MuxPacket) - sizeof(mPayloadSize);

  DataPacket(unsigned int aPayloadSIze, unsigned int aChan)
    : MuxPacket(sizeof(DataPacket) + aPayloadSIze, aChan, MuxCmd::Data)
    , mPayloadSize(aPayloadSIze)
  {
    assert(aPayloadSIze <= MaxPayloadSize);
  }

  static DataPacket* Create(unsigned int aPayloadSize, unsigned int aChan) {
    auto sz = sizeof(DataPacket) + aPayloadSize;
    auto buf = new char[sz];
    return new(buf) DataPacket(aPayloadSize, aChan);
  }
};

class Channel;

class PacketListener {
public:
  enum Error {
    Ok = 0,
    UnknownError,
    ProtocolError,
    NetworkError,
  };
  virtual void OnReceive(Channel* aChannel, const MuxPacket* aPacket) = 0;
  virtual void OnClose(Channel* aChannel) = 0;
  virtual void OnError(Channel* aChannel, Error aErr) = 0;
};

class Mux;
class StreamChannel;

class Channel {
public:
  typedef PacketListener::Error Error;

  enum Type {
    TypePacketChannel,
    TypeStreamChannel
  };

  static constexpr unsigned int first_seq = 1;

  Channel(unsigned int aId, unsigned int aSize, Mux *aMux, Type aType = TypePacketChannel)
    : mChanId(aId)
    , mSize(aSize)
    , mMux(aMux)
    , mType(aType)
    , mSeq(first_seq)
    , mSeqPeer(first_seq)
    , mValid(true) {}

  void SetListener(PacketListener* aListener) {
    mListener = aListener;
  }

  unsigned int GetChannelId() {
    return mChanId;
  }

  unsigned GetSize() { return mSize; }

  void Close();

  bool Send(MuxPacket* aPacket);

  // Handle incoing packets from Mux
  void HandleIncomingPacket(const MuxPacket* aPacket);

  Type GetChannelType() { return mType; }

  virtual StreamChannel* GetStream() { return nullptr; }

  void OnClose() {
    assert(mListener != nullptr);
    mListener->OnClose(this);
    mValid = false;
  }

  void OnError(Error aErr) {
    assert(mListener != nullptr);
    mListener->OnError(this, aErr);
    mValid = false;
  }

  bool IsValid() { return mValid; }

private:
  unsigned int mChanId;
  unsigned int mSize;

  Mux* mMux;
  Type mType;
  unsigned int mSeq;
  unsigned int mSeqPeer;
  PacketListener* mListener;
  bool mValid;
};

class StreamListener {
public:
  virtual void OnReceive(StreamChannel* aChannel, unsigned int aSize, const char *aData) = 0;
  virtual void OnClose(StreamChannel* aChannel) = 0;
  virtual void OnError(StreamChannel* aChannel, Channel::Error aErr) = 0;
};

class StreamChannel : private Channel, private PacketListener {
public:
  StreamChannel(unsigned int aId, Mux* aMux) : Channel(aId, 0, aMux, TypeStreamChannel) {
    Channel::SetListener(this);
  }

  void SetListener(StreamListener *aListener) {
    mListener = aListener;
  }

  void CLose() {
    Channel::Close();
  }

  bool Write(unsigned int aSize, const char *aData);

  virtual StreamChannel* GetStream() override { return this; }

  Channel* GetChannel() {
    return this;
  }

  bool IsValid() { return Channel::IsValid(); }

protected:
  StreamListener* mListener;

private:
  // Interface PacketListener
  virtual void OnReceive(Channel* aChannel, const MuxPacket* aPacket) override;
  virtual void OnClose(Channel* aChannel) override;
  virtual void OnError(Channel* aChannel, PacketListener::Error aErr) override;
};

class ChannelListener {
public:
  virtual void OnChannelOpened(unsigned int aSeq,
                               unsigned int aErr,
                               Channel* aChannel,
                               const char* aPath,
                               bool aFromPeer) = 0;
  virtual int OnCheckDataSize(const char* aPath) = 0;
};


class Transport {
public:
  virtual int Write(void *aData, int aSize) = 0;
};


/**
 * Interaction among Mux and other classes.
 *
 *                     PacketListener
 *                           ^
 *                           | <<inherit>>
 *                           |
 *               Write       |    HandleIncomingPacket
 * Transport <------------- Mux -----------------------> Channel
 *           ------------->  |
 *             ReceiveRaw    +-------> mControlChan: Channel
 *
 *  Mux -------> ChannelListener
 *
 *  Channel --------------> PacketListener
 *                               ^
 *                               | <<inherit>>
 *                               |
 *                          StreamChannel ------> StreamListener
 *
 */
class Mux : private PacketListener {
public:
  enum MuxSide {
    SideServer,
    SideClient
  };

  Mux(MuxSide aSide);

  void SetChanListener(ChannelListener* aListener) {
    mChanListener = aListener;
  }

  void SetTransport(Transport* aTransport) {
    mTransport = aTransport;
  }

  /**
   * Open a channel.
   *
   * A new channel will be created at the peer, and the
   * ChannelListener of this instance will receive the ID of the new
   * channel.
   *
   * \return a seq number that will be used by channel listeners to
   *         identify wha request a received channel ID belong to.
   */
  unsigned int Open(const char* aPath);

  // Send packets for the instances of Channel.
  bool DoSend(MuxPacket* aPacket);

  // Receive raw bytes from the Transport.
  bool ReceiveRaw(char *aData, unsigned int aSize);

  // Channel ask Mux to close itself.
  void ChannelAskClose(unsigned int aChanId);

  Channel* GetChannel(unsigned int aChanId) {
    if (aChanId == 0) {
      return &mControlChan;
    }
    auto chanitr = mChannels.find(aChanId);
    if (chanitr == mChannels.end()) {
      return nullptr;
    }
    return chanitr->second;
  }

private:
  // Handle incoming packets constructed by |ReceiveRaw()|.
  void HandleIncomingPacket(const MuxPacket* aPacket);
  bool DoCloseChannel(unsigned int aChanId);
  bool DoErrorChannel(unsigned int aChanId, Channel::Error aErr);
  bool SendNak(unsigned int aChanId, unsigned int aSeq);

  // Interface PacketListener
  virtual void OnReceive(Channel* aChannel, const MuxPacket* aPacket) override;
  virtual void OnClose(Channel* aChannel) override;
  virtual void OnError(Channel* aChannel, PacketListener::Error aErr) override;

  MuxSide mSide;
  Transport* mTransport;
  Channel mControlChan;
  ChannelListener *mChanListener;

  unsigned int mNextChanId;
  std::map<unsigned int, Channel*> mChannels;
  std::map<unsigned int, MuxPacket*> mWaitingPackets;
  int mInputBufSize;
  char mInputBuf[MuxPacket::MaxPacketSize];
};

#endif
