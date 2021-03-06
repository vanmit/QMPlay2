#ifndef DECODER_HPP
#define DECODER_HPP

#include <ModuleCommon.hpp>
#include <Packet.hpp>

#include <QByteArray>
#include <QString>

class QMPlay2_OSD;
class StreamInfo;
class Writer;
class LibASS;

class Decoder : public ModuleCommon
{
public:
	static Decoder *create( StreamInfo *streamInfo, Writer *writer = NULL, const QStringList &modNames = QStringList() );

	virtual QString name() const = 0;

	virtual bool aspect_ratio_changed() const { return false; }
	virtual Writer *HWAccel() const { return NULL; }

	/*
	 * hurry_up ==  0 -> no frame skipping, normal quality
	 * hurry_up >=  1 -> faster decoding, lower image quality, frame skipping during decode
	 * hurry_up == ~0 -> much faster decoding, no frame copying
	*/
	virtual int decode( Packet &packet, QByteArray &dest, bool flush = false, unsigned hurry_up = 0 ) = 0;
	virtual bool decodeSubtitle( const QByteArray &encoded, double pts, double pos, QMPlay2_OSD *&osd, int w, int h )
	{
		Q_UNUSED( encoded )
		Q_UNUSED( pts )
		Q_UNUSED( pos )
		Q_UNUSED( osd )
		Q_UNUSED( w )
		Q_UNUSED( h )
		return false;
	}

	virtual ~Decoder() {}
private:
	virtual bool open( StreamInfo *streamInfo, Writer *writer = NULL ) = 0;
protected:
	StreamInfo *streamInfo;
};

#endif
