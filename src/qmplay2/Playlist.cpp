#include <Playlist.hpp>

#include <QMPlay2Core.hpp>
#include <Functions.hpp>
#include <Module.hpp>
#include <Writer.hpp>
#include <Reader.hpp>

QList< Playlist::Entry > Playlist::read( const QString &url, QString *name )
{
	QList< Entry > list;
	Playlist *playlist = create( url, ReadOnly, name );
	if ( playlist )
	{
		list = playlist->_read();
		delete playlist;
	}
	return list;
}
bool Playlist::write( const QList< Entry > &list, const QString &url, QString *name )
{
	bool OK = false;
	Playlist *playlist = create( url, WriteOnly, name );
	if ( playlist )
	{
		OK = playlist->_write( list );
		delete playlist;
	}
	return OK;
}
QString Playlist::name( const QString &url )
{
	QString name;
	create( url, NoOpen, &name );
	return name;
}
QStringList Playlist::extensions()
{
	QStringList extensions;
	foreach ( Module *module, QMPlay2Core.getPluginsInstance() )
		foreach ( Module::Info mod, module->getModulesInfo() )
			if ( mod.type == Module::PLAYLIST )
				extensions += mod.extensions;
	return extensions;
}

Playlist *Playlist::create( const QString &url, OpenMode openMode, QString *name )
{
	QString extension = Functions::fileExt( url ).toLower();
	if ( extension.isEmpty() )
		return NULL;
	foreach ( Module *module, QMPlay2Core.getPluginsInstance() )
		foreach ( Module::Info mod, module->getModulesInfo() )
			if ( mod.type == Module::PLAYLIST && mod.extensions.contains( extension ) )
			{
				if ( openMode == NoOpen )
				{
					if ( name )
						*name = mod.name;
					return NULL;
				}
				Playlist *playlist = ( Playlist * )module->createInstance( mod.name );
				if ( !playlist )
					continue;
				switch ( openMode )
				{
					case ReadOnly:
						Reader::create( url, playlist->ioCtrl.toRef< Reader >() ); //TODO przerywanie (po co?)
						break;
					case WriteOnly:
						playlist->ioCtrl.assign( Writer::create( url ) );
						break;
					default:
						break;
				}
				if ( playlist->ioCtrl )
				{
					if ( name )
						*name = mod.name;
					return playlist;
				}
				delete playlist;
			}
	return NULL;
}
