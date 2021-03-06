#include <PlaylistWidget.hpp>

#include <QMPlay2Extensions.hpp>
#include <Functions.hpp>
#include <MenuBar.hpp>
#include <Demuxer.hpp>
#include <Reader.hpp>
#include <Main.hpp>

using Functions::chkMimeData;
using Functions::getUrlsFromMimeData;
using Functions::splitPrefixAndUrlIfHasPluginPrefix;

#include <QResizeEvent>
#include <QHeaderView>
#include <QFileInfo>
#include <QPainter>
#include <QMimeData>
#include <QDebug>
#include <QDrag>
#include <QMenu>
#include <QDir>

#define playlistMenu ( ( MenuBar::Playlist * )QMPlay2GUI.menubar->playlist )

static const int IconSize = 22;
static const int IconSize_2 = IconSize / 2;

/* UpdateEntryThr class */
void UpdateEntryThr::updateEntry( QTreeWidgetItem *item, const QString &name, int length )
{
	if ( !item )
		return;
	mutex.lock();
	if ( !isRunning() )
	{
		ioCtrl.resetAbort();
		start();
	}
	itemsToUpdate += ( ItemToUpdate ){ item, name, length };
	mutex.unlock();
}
void UpdateEntryThr::run()
{
	bool timeChanged = false;
	while ( !ioCtrl.isAborted() )
	{
		mutex.lock();
		if ( !itemsToUpdate.size() )
		{
			mutex.unlock();
			break;
		}
		ItemToUpdate itu = itemsToUpdate.dequeue();
		mutex.unlock();

		QTreeWidgetItem *tWI = itu.tWI;

		if ( !pLW.getChildren( PlaylistWidget::ONLY_NON_GROUPS ).contains( tWI ) )
			continue;

		QString url = pLW.getUrl( tWI );

		QString name = itu.name;
		int length = itu.length;

		bool updateTitle = true;
		if ( name.isNull() && length == -2 )
		{
			QImage img;
			Functions::getDataIfHasPluginPrefix( url, &url, &name, &img, &ioCtrl );
			pLW.setEntryIcon( img, tWI );

			IOController< Demuxer > &demuxer = ioCtrl.toRef< Demuxer >();
			if ( Demuxer::create( url, demuxer ) )
			{
				if ( name.isEmpty() )
					name = demuxer->title();
				length = demuxer->length();
				demuxer.clear();
			}
			else
				updateTitle = false;
		}

		if ( updateTitle )
			tWI->setText( 0, name.isEmpty() ? Functions::fileName( url, false ) : name );
		if ( length != tWI->data( 2, Qt::UserRole ).toInt() )
		{
			tWI->setText( 2, Functions::timeToStr( length ) );
			tWI->setData( 2, Qt::UserRole, QString::number( length ) );
			timeChanged = true;
		}
	}
	if ( timeChanged )
		pLW.ref( PlaylistWidget::REFRESH_GROUPS_TIME );
	pLW.viewport()->update();
}
void UpdateEntryThr::stop()
{
	ioCtrl.abort();
	wait( TERMINATE_TIMEOUT );
	if ( isRunning() )
	{
		terminate();
		wait( 1000 );
		ioCtrl.clear();
	}
}

/* AddThr class */
void AddThr::setData( const QStringList &_urls, QTreeWidgetItem *_par, bool _loadList, bool sync )
{
	ioCtrl.resetAbort();

	urls = _urls;
	par = _par;
	loadList = _loadList;

	firstI = NULL;
	lastI = pLW.currentItem();

	pLW.setItemsResizeToContents( false );

	emit pLW.visibleItemsCount( -1 );

	if ( sync )
	{
		while ( par->childCount() )
			delete par->child( 0 );
		pLW.ref( PlaylistWidget::REFRESH_CURRPLAYING );
	}

	emit QMPlay2Core.busyCursor();
	if ( !loadList )
	{
		pLW.rotation = 0;
		pLW.animationTimer.start( 50 );
		playlistMenu->stopLoading->setVisible( true );
		start();
	}
	else
		run();
}
void AddThr::setData( const QString &pth, QTreeWidgetItem *par ) //dla synchronizacji
{
	QStringList d_urls = QDir( pth ).entryList( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst );
	for ( int i = d_urls.size() - 1 ; i >= 0 ; --i )
		d_urls[ i ] = pth + d_urls[ i ];
	setData( d_urls, par, false, true );
}
void AddThr::stop()
{
	ioCtrl.abort();
	wait( TERMINATE_TIMEOUT );
	if ( isRunning() )
	{
		terminate();
		wait( 1000 );
		ioCtrl.clear();
	}
}
void AddThr::run()
{
	pLW._add( urls, par, &firstI, loadList );
	if ( currentThread() == pLW.thread() ) //jeżeli funkcja działa w głównym wątku
		finished();
}
void AddThr::finished()
{
	if ( !pLW.currPthToSave.isNull() )
	{
		QMPlay2GUI.setCurrentPth( pLW.currPthToSave );
		pLW.currPthToSave.clear();
	}
	pLW.animationTimer.stop();
	pLW.viewport()->repaint();
	pLW.setAnimated( false );
	pLW.ref();
	pLW.setItemsResizeToContents( true );
	if ( firstI && ( ( pLW.selectAfterAdd && pLW.currentItem() != firstI && pLW.currentItem() == lastI ) || !pLW.currentItem() ) )
		pLW.setCurrentItem( firstI );
	pLW.selectAfterAdd = false;
	pLW.setAnimated( true );
	pLW.processItems();
	emit pLW.returnItem( firstI );
	urls.clear();
	playlistMenu->stopLoading->setVisible( false );
	if ( !pLW.currentPlaying && !pLW.currentPlayingUrl.isEmpty() )
	{
		foreach ( QTreeWidgetItem *tWI, pLW.findItems( QString(), Qt::MatchRecursive | Qt::MatchContains ) )
			if ( pLW.getUrl( tWI ) == pLW.currentPlayingUrl )
			{
				pLW.setCurrentPlaying( tWI );
				break;
			}
	}
	emit QMPlay2Core.restoreCursor();
}

/*PlaylistWidget class*/
PlaylistWidget::PlaylistWidget() :
	addThr( *this ),
	updateEntryThr( *this ),
	repaintAll( false )
{
	setContextMenuPolicy( Qt::CustomContextMenu );
	setEditTriggers( QAbstractItemView::NoEditTriggers );
	setDragDropMode( QAbstractItemView::InternalMove );
	setAlternatingRowColors( true );
	setSelectionMode( QAbstractItemView::ExtendedSelection );
	setIndentation( 12 );
	setColumnCount( 3 );
	setAnimated( true );
	header()->setStretchLastSection( false );
	setHeaderHidden( true );
#if QT_VERSION < 0x050000
	header()->setResizeMode( 0, QHeaderView::Stretch );
#else
	header()->setSectionResizeMode( 0, QHeaderView::Stretch );
#endif
	header()->hideSection( 1 );
	setItemsResizeToContents( true );
	setIconSize( QSize( IconSize, IconSize ) );

	currentPlaying = NULL;
	Modifier = selectAfterAdd = hasHiddenItems = dontUpdateAfterAdd = false;

	connect( this, SIGNAL( insertItem( QTreeWidgetItem *, QTreeWidgetItem *, bool ) ), this, SLOT( insertItemSlot( QTreeWidgetItem *, QTreeWidgetItem *, bool ) ) );

	connect( this, SIGNAL( itemSelectionChanged() ), this, SLOT( modifyMenu() ) );
	connect( &updateEntryThr, SIGNAL( finished() ), this, SLOT( modifyMenu() ) );

	connect( this, SIGNAL( customContextMenuRequested( const QPoint & ) ), this, SLOT( popupContextMenu( const QPoint & ) ) );
	connect( this, SIGNAL( setItemIcon( QTreeWidgetItem *, const QImage & ) ), this, SLOT( setItemIconSlot( QTreeWidgetItem *, const QImage & ) ) );
	connect( &animationTimer, SIGNAL( timeout() ), this, SLOT( animationUpdate() ) );
	connect( &addTimer, SIGNAL( timeout() ), this, SLOT( addTimerElapsed() ) );

//	rotation = 0;
//	animationTimer.start( 50 );
}

void PlaylistWidget::setItemsResizeToContents( bool b )
{
	const QHeaderView::ResizeMode rm = b ? QHeaderView::ResizeToContents : QHeaderView::Fixed;
	for ( int i = 1 ; i <= 2 ; ++i )
#if QT_VERSION < 0x050000
		header()->setResizeMode( i, rm );
#else
		header()->setSectionResizeMode( i, rm );
#endif
}

bool PlaylistWidget::add( const QStringList &urls, QTreeWidgetItem *par, bool loadList )
{
	if ( urls.isEmpty() )
		return false;
	if ( canModify() )
		addThr.setData( urls, par, loadList );
	else
	{
		enqueuedAddData += ( AddData ){ urls, par, loadList };
		addTimer.start();
	}
	return true;
}
bool PlaylistWidget::add( const QStringList &urls, bool atEndOfList )
{
	selectAfterAdd = true;
	return add( urls, atEndOfList ? NULL : ( !selectedItems().count() ? NULL : currentItem() ), false );
}
void PlaylistWidget::sync( const QString &pth, QTreeWidgetItem *par )
{
	if ( !canModify() )
		return;
	addThr.setData( pth, par );
}

void PlaylistWidget::setCurrentPlaying( QTreeWidgetItem *tWI )
{
	if ( !tWI )
		return;
	currentPlaying = tWI;
	/* Ikona */
	currentPlayingItemIcon = currentPlaying->icon( 0 );
	QIcon play_icon = QMPlay2Core.getIconFromTheme( "media-playback-start" );
	if ( !play_icon.availableSizes().isEmpty() && !play_icon.availableSizes().contains( QSize( IconSize, IconSize ) ) )
	{
		QPixmap pix( IconSize, IconSize );
		QPixmap playPix = play_icon.pixmap( play_icon.availableSizes()[ 0 ] );
		pix.fill( Qt::transparent );
		QPainter p( &pix );
		p.drawPixmap( IconSize_2- playPix.width() / 2, IconSize_2 - playPix.height() / 2, playPix );
		play_icon = QIcon( pix );
	}
	currentPlaying->setIcon( 0, play_icon );
}

void PlaylistWidget::clear()
{
	if ( !canModify() )
		return;
	_queue.clear();
	clearCurrentPlaying();
	QTreeWidget::clear();
}
void PlaylistWidget::clearCurrentPlaying( bool url )
{
	currentPlaying = NULL;
	if ( url )
		currentPlayingUrl.clear();
	currentPlayingItemIcon = QIcon();
	modifyMenu();
}

QList< QTreeWidgetItem * > PlaylistWidget::topLevelNonGroupsItems() const
{
	QList< QTreeWidgetItem * > items;
	int count = topLevelItemCount();
	for ( int i = 0 ; i < count ; i++ )
	{
		QTreeWidgetItem *tWI = topLevelItem( i );
		if ( !tWI->parent() && !isGroup( tWI ) )
			items += tWI;
	}
	return items;
}
QList< QUrl > PlaylistWidget::getUrls() const
{
	QList< QUrl > urls;
	foreach ( QTreeWidgetItem *tWI, selectedItems() )
	{
		QString url = getUrl( tWI );
		if ( !url.isEmpty() && !url.contains( "://{" ) )
		{
#ifdef Q_OS_WIN
			if ( url.mid( 4, 5 ) != ":////" )
				url.replace( "file://", "file:///" );
#endif
			urls += url;
		}
	}
	return urls;
}

QTreeWidgetItem *PlaylistWidget::newGroup( const QString &name, const QString &url, QTreeWidgetItem *parent, bool insertChildAt0Idx )
{
	QTreeWidgetItem *tWI = new QTreeWidgetItem;

	tWI->setFlags( tWI->flags() | Qt::ItemIsEditable );
	tWI->setIcon( 0, url.isEmpty() ? *QMPlay2GUI.groupIcon : *QMPlay2GUI.folderIcon );
	tWI->setText( 0, name );
	tWI->setData( 0, Qt::UserRole, url );

	emit insertItem( tWI, parent, insertChildAt0Idx );
	return tWI;
}
QTreeWidgetItem *PlaylistWidget::newGroup()
{
	return newGroup( QString(), QString(), !selectedItems().count() ? NULL : currentItem(), true );
}

QTreeWidgetItem *PlaylistWidget::newEntry( const Playlist::Entry &entry, QTreeWidgetItem *parent )
{
	QTreeWidgetItem *tWI = new QTreeWidgetItem;

	QImage img;
	Functions::getDataIfHasPluginPrefix( entry.url, NULL, NULL, &img );
	setEntryIcon( img, tWI );

	tWI->setFlags( tWI->flags() &~ Qt::ItemIsDropEnabled );
	tWI->setText( 0, entry.name );
	tWI->setData( 0, Qt::UserRole, entry.url );
	tWI->setText( 2, Functions::timeToStr( entry.length ) );
	tWI->setData( 2, Qt::UserRole, QString::number( entry.length ) );

	emit insertItem( tWI, parent, false );
	return tWI;
}

QList <QTreeWidgetItem * > PlaylistWidget::getChildren( CHILDREN children, const QTreeWidgetItem *parent ) const
{
	QList <QTreeWidgetItem * > list;
	if ( !parent )
		parent = invisibleRootItem();
	int count = parent->childCount();
	for ( int i = 0 ; i < count ; i++ )
	{
		QTreeWidgetItem *tWI = parent->child( i );
		bool group = isGroup( tWI );
		if
		(
			( ( children & ONLY_NON_GROUPS ) && !group ) ||
			( ( children & ONLY_GROUPS )     &&  group )
		)
			list += tWI;
		if ( group )
			list += getChildren( children, tWI );
	}
	return list;
}

bool PlaylistWidget::canModify( bool all ) const
{
	return !( addThr.isRunning() || ( all ? QAbstractItemView::state() != 0 : false ) || updateEntryThr.isRunning() );
}

void PlaylistWidget::queue()
{
	foreach ( QTreeWidgetItem *tWI, selectedItems() )
	{
		if ( isGroup( tWI ) )
			continue;
		if ( _queue.contains( tWI ) )
		{
			tWI->setText( 1, QString() );
			_queue.removeOne( tWI );
		}
		else
			_queue += tWI;
	}
	ref( REFRESH_QUEUE );
}
void PlaylistWidget::ref( REFRESH Refresh )
{
	QList< QTreeWidgetItem * > items = getChildren( ONLY_NON_GROUPS );
	if ( Refresh & REFRESH_QUEUE )
	{
		for ( int i = 0 ; i < _queue.size() ; i++ )
		{
			if ( !items.contains( _queue[ i ] ) )
				_queue.removeAt( i-- );
			else
				_queue[ i ]->setText( 1, QString::number( i + 1 ) );
		}
		_queue.size() ? header()->showSection( 1 ) : header()->hideSection( 1 );
	}
	if ( Refresh & REFRESH_GROUPS_TIME )
	{
		QList< QTreeWidgetItem * > groups = getChildren( ONLY_GROUPS );
		for ( int i = groups.size() - 1 ; i >= 0 ; i-- )
		{
			int length = 0;
			foreach ( QTreeWidgetItem *tWI, getChildren( ALL_CHILDREN, groups[ i ] ) )
			{
				if ( tWI->parent() != groups[ i ] )
					continue;
				int l = tWI->data( 2, Qt::UserRole ).toInt();
				if ( l > 0 )
					length += l;
			}
			groups[ i ]->setText( 2, length ? Functions::timeToStr( length ) : QString() );
			groups[ i ]->setData( 2, Qt::UserRole, length ? QString::number( length ) : QVariant() );
		}
	}
	if ( ( Refresh & REFRESH_CURRPLAYING ) && !items.contains( currentPlaying ) )
		clearCurrentPlaying( false );
}

void PlaylistWidget::processItems( QList< QTreeWidgetItem * > *itemsToShow, bool hideGroups )
{
	int count = 0;
	hasHiddenItems = false;
	foreach ( QTreeWidgetItem *tWI, getChildren( PlaylistWidget::ONLY_NON_GROUPS, NULL ) )
	{
		if ( itemsToShow )
			tWI->setHidden( !itemsToShow->contains( tWI ) );
		if ( !tWI->isHidden() )
			count++;
		else
			hasHiddenItems = true;
	}
	emit visibleItemsCount( count );

	if ( !itemsToShow )
		return;

	//ukrywanie pustych grup
	QList< QTreeWidgetItem * > groups = getChildren( ONLY_GROUPS );
	for ( int i = groups.size() - 1 ; i >= 0 ; i-- )
	{
		if ( hideGroups )
		{
			bool hasVisibleItems = false;
			foreach ( QTreeWidgetItem *tWI, getChildren( ONLY_NON_GROUPS, groups[ i ] ) )
			{
				if ( !tWI->isHidden() )
				{
					hasVisibleItems = true;
					break;
				}
			}
			groups[ i ]->setHidden( !hasVisibleItems );
		}
		else
			groups[ i ]->setHidden( false );
	}
}

void PlaylistWidget::_add( const QStringList &urls, QTreeWidgetItem *parent, QTreeWidgetItem **firstI, bool loadList )
{
	int size = urls.size();
	for ( int i = 0 ; i < size ; i++ )
	{
		if ( addThr.ioCtrl.isAborted() )
			break;
		QTreeWidgetItem *tWI = parent;
		QString url = Functions::Url( urls[ i ] ), name;
		QList< Playlist::Entry > entries = Playlist::read( url, &name );
		if ( !name.isEmpty() ) //ładowanie playlisty
		{
			if ( !loadList )
				tWI = newGroup( Functions::fileName( url, false ), QString(), tWI ); //dodawanie grupy playlisty, jeżeli jest dodawana, a nie ładowana
			else
				clear(); //wykonać można tylko z głównego wątku!
			QList< QTreeWidgetItem * > groupList;
			int queueSize = _queue.size();
			foreach ( Playlist::Entry entry, entries )
			{
				QTreeWidgetItem *I = NULL, *par = NULL;
				int idx = entry.parent - 1;
				if ( idx >= 0 && groupList.size() > idx )
					par = groupList[ idx ];
				else
					par = tWI;
				if ( entry.GID )
					groupList += newGroup( entry.name, entry.url, par );
				else
				{
					I = newEntry( entry, par );
					if ( entry.queue ) //odbudowywanie kolejki
					{
						for ( int j = _queue.size() ; j <= queueSize + entry.queue - 1 ; j++ )
							_queue += NULL;
						_queue[ queueSize + entry.queue - 1 ] = I;
					}
					if ( firstI && !*firstI )
						*firstI = I;
				}
				if ( entry.selected && firstI )
					*firstI = entry.GID ? groupList.last() : I;
			}
			if ( loadList )
				break;
		}
		else if ( !loadList )
		{
			QString dUrl = ( url.left( 7 ) == "file://" ) ? url.mid( 7 ) : QString();
			if ( QFileInfo( dUrl ).isDir() ) //dodawanie podkatalogu
			{
				if ( currPthToSave.isNull() )
					currPthToSave = dUrl;
				QStringList d_urls = QDir( dUrl ).entryList( QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst );
				if ( d_urls.size() )
				{
					url = Functions::cleanPath( url );
					QTreeWidgetItem *p = newGroup( Functions::fileName( url ), QString(), tWI );
					for ( int j = d_urls.size() - 1 ; j >= 0 ; j-- )
					{
						d_urls[ j ].prepend( url );
#ifdef Q_OS_WIN
						d_urls[ j ].replace( "file://", "file:///" );
#endif
					}
					_add( d_urls, p, firstI );
				}
			}
			else
			{
				Playlist::Entry entry;
				entry.url = url;

				bool ok = false;
				if ( dontUpdateAfterAdd )
				{
					dontUpdateAfterAdd = false;
					ok = true;
				}
				else
				{
					Functions::getDataIfHasPluginPrefix( url, &url, &entry.name, NULL, &addThr.ioCtrl );
					IOController< Demuxer > &demuxer = addThr.ioCtrl.toRef< Demuxer >();
					if ( Demuxer::create( url, demuxer ) )
					{
						if ( currPthToSave.isNull() && QFileInfo( dUrl ).isFile() )
							currPthToSave = Functions::filePath( dUrl );
						if ( entry.name.isEmpty() )
							entry.name = demuxer->title();
						entry.length = demuxer->length();
						demuxer.clear();
						ok = true;
					}
				}
				if ( ok )
				{
					if ( entry.name.isEmpty() )
						entry.name = Functions::fileName( url, false );

					tWI = newEntry( entry, tWI );
					if ( firstI && !*firstI )
						*firstI = tWI;
				}
			}
		}
	}
}

void PlaylistWidget::setEntryIcon( QImage &img, QTreeWidgetItem *tWI )
{
	if ( img.isNull() )
	{
		if ( tWI == currentPlaying )
			currentPlayingItemIcon = *QMPlay2GUI.mediaIcon;
		else
			tWI->setIcon( 0, *QMPlay2GUI.mediaIcon );
	}
	else
	{
		if ( img.height() == img.width() && img.height() > IconSize && img.width() > IconSize )
			img = img.scaled( IconSize, IconSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
		else if ( img.height() > img.width() && img.height() > IconSize )
			img = img.scaledToHeight( IconSize, Qt::SmoothTransformation );
		else if ( img.width() > img.height() && img.width() > IconSize )
			img = img.scaledToWidth( IconSize, Qt::SmoothTransformation );
		if ( tWI == currentPlaying )
			emit setItemIcon( NULL, img ); //currentPlayingItemIcon
		else
			emit setItemIcon( tWI, img );
	}
}

void PlaylistWidget::mouseMoveEvent( QMouseEvent *e )
{
	if ( e->buttons() & Qt::MidButton || ( ( e->buttons() & Qt::LeftButton ) && Modifier ) )
	{
		QMimeData *mimeData = new QMimeData;
		mimeData->setUrls( getUrls() );
		if ( mimeData->urls().isEmpty() )
		{
			delete mimeData;
			return;
		}
		QDrag *drag = new QDrag( this );
		drag->setMimeData( mimeData );

		QPixmap pix;
		QTreeWidgetItem *currI = currentItem();
		if ( currI && currI != currentPlaying )
			pix = currI->icon( 0 ).pixmap( IconSize, IconSize );
		if ( pix.isNull() )
			pix = QMPlay2Core.getIconFromTheme( "applications-multimedia" ).pixmap( IconSize, IconSize );
		drag->setPixmap( pix );

		drag->exec( Qt::CopyAction | Qt::MoveAction | Qt::LinkAction );
	}
	else if ( canModify( false ) && !hasHiddenItems )
	{
		internalDrag = false;
		QTreeWidget::mouseMoveEvent( e );
		if ( internalDrag )
			ref();
	}
}
void PlaylistWidget::dragEnterEvent( QDragEnterEvent *e )
{
	if ( addThr.isRunning() || !e )
		return;
	if ( chkMimeData( e->mimeData() ) )
		e->accept();
	else
	{
		QTreeWidget::dragEnterEvent( e );
		internalDrag = true;
	}
}
void PlaylistWidget::dragMoveEvent( QDragMoveEvent *e )
{
	if ( !e )
		return;
	if ( e->source() == this )
		QTreeWidget::dragMoveEvent( e );
	else
		setCurrentItem( itemAt( e->pos() ) );
}
void PlaylistWidget::dropEvent( QDropEvent *e )
{
	if ( !e )
		return;
	if ( e->source() == this )
	{
		QTreeWidgetItem *cI = currentItem();
		setItemsResizeToContents( false );
		QTreeWidget::dropEvent( e );
		setItemsResizeToContents( true );
		setCurrentItem( cI );
	}
	else if ( e->mimeData() )
	{
		selectAfterAdd = true;
		add( getUrlsFromMimeData( e->mimeData() ), itemAt( e->pos() ) );
	}
}
void PlaylistWidget::keyPressEvent( QKeyEvent *e )
{
	Modifier = e->modifiers() == Qt::MetaModifier || e->modifiers() == Qt::AltModifier;
	QTreeWidget::keyPressEvent( e );
}
void PlaylistWidget::keyReleaseEvent( QKeyEvent *e )
{
	Modifier = false;
	QTreeWidget::keyReleaseEvent( e );
}
void PlaylistWidget::paintEvent( QPaintEvent *e )
{
	QTreeWidget::paintEvent( e );
	if ( !animationTimer.isActive() )
		return;

	QPainter p( viewport() );
	p.setRenderHint( QPainter::Antialiasing );

	QRect arcRect = getArcRect( 48 );
	p.drawArc( arcRect, rotation * 16, 120 * 16 );
	p.drawArc( arcRect, ( rotation + 180 ) * 16, 120 * 16 );
}
void PlaylistWidget::scrollContentsBy( int dx, int dy )
{
	if ( dx || dy )
		repaintAll = true;
	QAbstractScrollArea::scrollContentsBy( dx, dy );
}

QRect PlaylistWidget::getArcRect( int size )
{
	return QRect( QPoint( width() / 2 - size / 2, height() / 2 - size / 2 ), QSize( size, size ) );
}

void PlaylistWidget::insertItemSlot( QTreeWidgetItem *tWI, QTreeWidgetItem *parent, bool insertChildAt0Idx )
{
	QTreeWidgetItem *cI = parent;
	if ( parent && !isGroup( parent ) )
		parent = parent->parent();
	if ( cI && cI->parent() == parent )
	{
		if ( parent )
			parent->insertChild( parent->indexOfChild( cI ) + 1, tWI );
		else
			insertTopLevelItem( indexOfTopLevelItem( cI ) + 1, tWI );
	}
	else
	{
		if ( parent )
		{
			if ( !insertChildAt0Idx )
				parent->addChild( tWI );
			else
				parent->insertChild( 0,tWI );
		}
		else
			addTopLevelItem( tWI );
	}
}
void PlaylistWidget::popupContextMenu( const QPoint &p )
{
	playlistMenu->popup( mapToGlobal( p ) );
}
void PlaylistWidget::setItemIconSlot( QTreeWidgetItem *tWI, const QImage &img )
{
	if ( img.width() == IconSize && img.height() == IconSize )
	{
		if ( tWI )
			tWI->setIcon( 0, QPixmap::fromImage( img ) );
		else
			currentPlayingItemIcon = QPixmap::fromImage( img );
	}
	else
	{
		QPixmap canvas( IconSize, IconSize );
		canvas.fill( QColor( 0, 0, 0, 0 ) );
		QPainter p( &canvas );
		p.drawImage( IconSize_2 - img.width() / 2, IconSize_2 - img.height() / 2, img );
		p.end();
		if ( tWI )
			tWI->setIcon( 0, canvas );
		else
			currentPlayingItemIcon = canvas;
	}
}
void PlaylistWidget::animationUpdate()
{
	rotation -= 6;
	if ( !repaintAll )
		viewport()->repaint( getArcRect( 50 ) );
	else
	{
		viewport()->repaint();
		repaintAll = false;
	}
}
void PlaylistWidget::addTimerElapsed()
{
	if ( canModify() && !enqueuedAddData.isEmpty() )
	{
		AddData addData = enqueuedAddData.dequeue();
		addThr.setData( addData.urls, addData.par, addData.loadList );
	}
	if ( enqueuedAddData.isEmpty() )
		addTimer.stop();
}

void PlaylistWidget::modifyMenu()
{
	QString entryUrl = getUrl();
	QString entryName;
	int entryLength = -2;
	if ( currentItem() )
	{
		entryName = currentItem()->text( 0 );
		entryLength = currentItem()->data( 2, Qt::UserRole ).toInt();
	}

	playlistMenu->sync->setVisible( currentItem() && isGroup( currentItem() ) && !entryUrl.isEmpty() );
	playlistMenu->renameGroup->setVisible( currentItem() && isGroup( currentItem() ) );
	playlistMenu->entryProperties->setVisible( currentItem() );
	playlistMenu->queue->setVisible( currentItem() );
	playlistMenu->goToPlaying->setVisible( currentPlaying );
	playlistMenu->copy->setVisible( selectedItems().count() );

	playlistMenu->extensions->clear();
	foreach ( QMPlay2Extensions *QMPlay2Ext, QMPlay2Extensions::QMPlay2ExtensionsList() )
	{
		QString addressPrefixName, url, param;
		QAction *act;
		if ( splitPrefixAndUrlIfHasPluginPrefix( entryUrl, &addressPrefixName, &url, &param ) )
			act = QMPlay2Ext->getAction( entryName, entryLength, url, addressPrefixName, param );
		else
			act = QMPlay2Ext->getAction( entryName, entryLength, entryUrl );
		if ( act )
		{
			act->setParent( playlistMenu->extensions );
			playlistMenu->extensions->addAction( act );
		}
	}
	playlistMenu->extensions->setEnabled( playlistMenu->extensions->actions().count() );
}
