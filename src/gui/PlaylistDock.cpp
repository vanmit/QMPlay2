#include <PlaylistDock.hpp>
#include <PlaylistWidget.hpp>

#include <EntryProperties.hpp>
#include <Functions.hpp>
#include <LineEdit.hpp>
#include <Main.hpp>

using Functions::getUrlsFromMimeData;
using Functions::chkMimeData;

#include <QFileInfo>
#include <QLabel>
#include <QBuffer>
#include <QUrl>
#include <QLineEdit>
#include <QGridLayout>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QPainter>
#include <QAction>
#include <QMessageBox>
#include <QDebug>

PlaylistDock::PlaylistDock()
{
	setWindowTitle( tr( "Playlista" ) );

	lastPlaying = NULL;

	setWidget( &mainW );

	list = new PlaylistWidget;
	findE = new LineEdit;
	findE->setToolTip( tr( "Filtruj wpisy" ) );
	statusL = new QLabel;

	layout = new QGridLayout( &mainW );
	layout->addWidget( list );
	layout->addWidget( findE );
	layout->addWidget( statusL );

	playAfterAdd = false;

	connect( list, SIGNAL( itemDoubleClicked( QTreeWidgetItem *, int ) ), this, SLOT( itemDoubleClicked( QTreeWidgetItem * ) ) );
	connect( list, SIGNAL( returnItem( QTreeWidgetItem * ) ), this, SLOT( addAndPlay( QTreeWidgetItem * ) ) );
	connect( list, SIGNAL( visibleItemsCount( int ) ), this, SLOT( visibleItemsCount( int ) ) );
	connect( findE, SIGNAL( textChanged( const QString & ) ), this, SLOT( findItems( const QString & ) ) );
	connect( findE, SIGNAL( returnPressed() ), this, SLOT( findNext() ) );

	QAction *act = new QAction( this );
	act->setShortcuts( QList< QKeySequence >() << QKeySequence( "Return" ) << QKeySequence( "Enter" ) );
	connect( act, SIGNAL( triggered() ), this, SLOT( start() ) );
	act->setShortcutContext( Qt::WidgetWithChildrenShortcut );
	addAction( act );
}

void PlaylistDock::stopThreads()
{
	list->updateEntryThr.stop();
	list->addThr.stop();
}

QString PlaylistDock::getUrl( QTreeWidgetItem *tWI ) const
{
	return list->getUrl( tWI );
}

void PlaylistDock::load( const QString &url )
{
	if ( !url.isEmpty() )
		list->add( QStringList( url ), NULL, true );
}
bool PlaylistDock::save( const QString &_url )
{
	QString url = Functions::Url( _url );
	QList< QTreeWidgetItem * > parents;
	QList< Playlist::Entry > entries;
	foreach ( QTreeWidgetItem *tWI, list->getChildren() )
	{
		Playlist::Entry entry;
		if ( PlaylistWidget::isGroup( tWI ) )
		{
			parents += tWI;
			entry.GID = parents.size();
		}
		else
		{
			entry.length = tWI->data( 2, Qt::UserRole ).isNull() ? -1 : tWI->data( 2, Qt::UserRole ).toInt();
			entry.queue = tWI->text( 1 ).toInt();
		}
		entry.url = tWI->data( 0, Qt::UserRole ).toString();
		entry.name = tWI->text( 0 );
		if ( tWI->parent() )
			entry.parent = parents.indexOf( tWI->parent() ) + 1;
		entry.selected = tWI == list->currentItem();
		entries += entry;
	}
	return Playlist::write( entries, url );
}

void PlaylistDock::add( const QStringList &urls )
{
	list->add( urls );
}
void PlaylistDock::addAndPlay( const QStringList &urls )
{
	playAfterAdd = true;
	list->dontUpdateAfterAdd = urls.size() == 1;
	list->add( urls, true );
}
void PlaylistDock::add( const QString &url )
{
	if ( !url.isEmpty() )
		add( QStringList( url ) );
}
void PlaylistDock::addAndPlay( const QString &url )
{
	if ( url.isEmpty() )
		return;
	/* Jeżeli wpis istnieje, znajdzie go i zacznie odtwarzać */
	QList< QTreeWidgetItem * > items = list->getChildren( PlaylistWidget::ONLY_NON_GROUPS );
	QString tmpUrl = Functions::Url( url );
	foreach ( QTreeWidgetItem *item, items )
	{
		if ( item->data( 0, Qt::UserRole ).toString() == tmpUrl )
		{
			playAfterAdd = true;
			addAndPlay( item );
			return;
		}
	}
	/**/
	addAndPlay( QStringList( url ) );
}

void PlaylistDock::delEntries( const QStringList &entries )
{
	if ( entries.isEmpty() || !list->canModify() ) //jeżeli jest np. drag and drop to nie wolno usuwać
		return;
	QList< QTreeWidgetItem * > allItems = list->getChildren( PlaylistWidget::ONLY_NON_GROUPS );
	if ( !allItems.isEmpty() )
	{
		list->setItemsResizeToContents( false );
		foreach ( QTreeWidgetItem *tWI, allItems )
		{
			if ( entries.contains( tWI->data( 0, Qt::UserRole ).toString() ) )
			{
				randomPlayedItems.removeOne( tWI );
				delete tWI;
			}
		}
		list->ref();
		list->setItemsResizeToContents( true );
		list->processItems();
	}
}

void PlaylistDock::expandTree( QTreeWidgetItem *i )
{
	while ( i )
	{
		if ( !i->isExpanded() )
			i->setExpanded( true );
		i = i->parent();
	}
}

void PlaylistDock::itemDoubleClicked( QTreeWidgetItem *tWI )
{
	if ( !tWI || PlaylistWidget::isGroup( tWI ) )
	{
		if ( !tWI )
			emit stop();
		return;
	}
	if ( !list->currentPlaying || list->currentItem() == list->currentPlaying )
		list->setCurrentItem( tWI );
	lastPlaying = tWI;

	//Jeżeli jest w kolejce, usuń z kolejki
	int idx = list->_queue.indexOf( tWI );
	if ( idx >= 0 )
	{
		tWI->setText( 1, QString() );
		list->_queue.removeOne( tWI );
		list->ref( PlaylistWidget::REFRESH_QUEUE );
	}

	emit play( tWI->data( 0, Qt::UserRole ).toString() );
}
void PlaylistDock::addAndPlay( QTreeWidgetItem *tWI )
{
	if ( !tWI || !playAfterAdd )
		return;
	playAfterAdd = false;
	list->setCurrentItem( tWI );
	start();
}

void PlaylistDock::stopLoading()
{
	list->addThr.stop();
}
void PlaylistDock::next()
{
	QList< QTreeWidgetItem * > l = list->getChildren( PlaylistWidget::ONLY_NON_GROUPS );
	if ( !l.contains( lastPlaying ) )
		lastPlaying = NULL;
	QTreeWidgetItem *tWI = NULL;
	if ( !l.isEmpty() )
	{
		if ( repeatMode == Random || repeatMode == RandomGroup )
		{
			if ( repeatMode == RandomGroup )
			{
				QTreeWidgetItem *P = list->currentPlaying ? list->currentPlaying->parent() : ( list->currentItem() ? list->currentItem()->parent() : NULL );
				expandTree( P );
				l = P ? list->getChildren( PlaylistWidget::ONLY_NON_GROUPS, P ) : list->topLevelNonGroupsItems();
				if ( l.isEmpty() || ( !randomPlayedItems.isEmpty() && randomPlayedItems[ 0 ]->parent() != P ) )
					randomPlayedItems.clear();
			}
			if ( randomPlayedItems.count() == l.count() ) //odtworzono całą playlistę
				randomPlayedItems.clear();
			else
			{
				int r;
				if ( l.count() == 1 )
					r = 0;
				else
				{
					do
						r = qrand() % l.count();
					while ( lastPlaying == l[ r ] || randomPlayedItems.contains( l[ r ] ) );
				}
				randomPlayedItems.append( ( tWI = l[ r ] ) );
			}
		}
		else
		{
			bool canRepeat = sender() ? !qstrcmp( sender()->metaObject()->className(), "PlayClass" ) : false;
			if ( canRepeat && repeatMode == RepeatEntry ) //zapętlenie utworu
				tWI = lastPlaying ? lastPlaying : list->currentItem();
			else
			{
				QTreeWidgetItem *P = NULL;
				if ( !list->_queue.size() )
				{
					tWI = list->currentPlaying ? list->currentPlaying : list->currentItem();
					P = tWI ? tWI->parent() : NULL;
					expandTree( P );
					for ( ;; )
					{
						tWI = list->itemBelow( tWI );
						if ( canRepeat && repeatMode == RepeatGroup && P && ( !tWI || tWI->parent() != P ) ) //zapętlenie grupy
						{
							QList< QTreeWidgetItem * > l = list->getChildren( PlaylistWidget::ONLY_NON_GROUPS, P );
							if ( !l.isEmpty() )
								tWI = l[ 0 ];
							break;
						}
						if ( PlaylistWidget::isGroup( tWI ) ) //grupa
						{
							if ( !tWI->isExpanded() )
								tWI->setExpanded( true );
							continue;
						}
						break;
					}
				}
				else
					tWI = list->_queue.first();
				if ( canRepeat && ( repeatMode == RepeatList || repeatMode == RepeatGroup ) && !tWI && !l.isEmpty() ) //zapętlenie listy
					tWI = l[ 0 ];
			}
		}
	}
	itemDoubleClicked( tWI );
}
void PlaylistDock::prev()
{
	QTreeWidgetItem *tWI = NULL;
//Dla "wstecz" nie uwzględniam kolejkowania utworów
	tWI = list->currentPlaying ? list->currentPlaying : list->currentItem();
	if ( tWI )
		expandTree( tWI->parent() );
	for ( ;; )
	{
		QTreeWidgetItem *tmpI = list->itemAbove( tWI );
		if ( PlaylistWidget::isGroup( tmpI ) && !tmpI->isExpanded() )
		{
			tmpI->setExpanded( true );
			continue;
		}
		tWI = list->itemAbove( tWI );
		if ( !PlaylistWidget::isGroup( tWI ) )
			break;
	}
	itemDoubleClicked( tWI );
}
void PlaylistDock::start()
{
	itemDoubleClicked( list->currentItem() );
}
void PlaylistDock::clearCurrentPlaying()
{
	if ( list->currentPlaying )
		list->currentPlaying->setIcon( 0, list->currentPlayingItemIcon );
	list->clearCurrentPlaying();
}
void PlaylistDock::setCurrentPlaying()
{
	if ( !lastPlaying || PlaylistWidget::isGroup( lastPlaying ) )
		return;
	list->currentPlayingUrl = getUrl( lastPlaying );
	list->setCurrentPlaying( lastPlaying );
}
void PlaylistDock::newGroup()
{
	list->setCurrentItem( list->newGroup() );
	entryProperties();
}

void PlaylistDock::delEntries()
{
	if ( !isVisible() || !list->canModify() ) //jeżeli jest np. drag and drop to nie wolno usuwać
		return;
	QList< QTreeWidgetItem * > selectedItems = list->selectedItems();
	if ( selectedItems.size() )
	{
		list->setItemsResizeToContents( false );
		QTreeWidgetItem *par = list->currentItem() ? list->currentItem()->parent() : NULL;
		foreach ( QTreeWidgetItem *tWI, selectedItems )
			if ( list->getChildren().contains( tWI ) )
			{
				randomPlayedItems.removeOne( tWI );
				delete tWI;
			}
		list->ref();
		if ( list->currentItem() )
			par = list->currentItem();
		else if ( !list->getChildren().contains( par ) )
			par = NULL;
		if ( !par )
			par = list->topLevelItem( 0 );
		list->setCurrentItem( par );
		list->setItemsResizeToContents( true );
		list->processItems();
	}
}
void PlaylistDock::delNonGroupEntries()
{
	if ( !list->canModify() ) //jeżeli jest np. drag and drop to nie wolno usuwać
		return;
	if ( QMessageBox::question( this, tr( "Playlista" ), tr( "Czy na pewno chcesz usunąć niepogrupowane wpisy?" ), QMessageBox::Yes, QMessageBox::No ) == QMessageBox::Yes )
	{
		list->setItemsResizeToContents( false );
		foreach ( QTreeWidgetItem *tWI, list->topLevelNonGroupsItems() )
		{
			randomPlayedItems.removeOne( tWI );
			delete tWI;
		}
		list->ref();
		list->setItemsResizeToContents( true );
		list->processItems();
	}
}
void PlaylistDock::clear()
{
	if ( QMessageBox::question( this, tr( "Playlista" ), tr( "Czy na pewno chcesz wyczyścić listę?" ), QMessageBox::Yes, QMessageBox::No ) == QMessageBox::Yes )
		list->clear();
}
void PlaylistDock::copy()
{
	QMimeData *mimeData = new QMimeData;
	mimeData->setUrls( list->getUrls() );
	if ( mimeData->urls().isEmpty() )
	{
		qApp->clipboard()->clear();
		delete mimeData;
		return;
	}
	qApp->clipboard()->setMimeData( mimeData );
}
void PlaylistDock::paste()
{
	const QMimeData *mimeData = qApp->clipboard()->mimeData();
	if ( chkMimeData( mimeData ) )
		list->add( getUrlsFromMimeData( mimeData ), list->selectedItems().count() ? list->currentItem() : NULL );
}
void PlaylistDock::renameGroup()
{
	QTreeWidgetItem *tWI = list->currentItem();
	if ( !PlaylistWidget::isGroup( tWI ) )
		return;
	list->editItem( tWI );
}
void PlaylistDock::entryProperties()
{
	bool sync, accepted;
	EntryProperties eP( this, list->currentItem(), sync, accepted );
	if ( accepted )
	{
		if ( sync )
			syncCurrentFolder();
		list->modifyMenu();
		list->updateEntryThr.updateEntry( list->currentItem() );
	}
}
void PlaylistDock::timeSort1()
{
	list->sortItems( 2, Qt::AscendingOrder );
	list->scrollToItem( list->currentItem() );
}
void PlaylistDock::timeSort2()
{
	list->sortItems( 2, Qt::DescendingOrder );
	list->scrollToItem( list->currentItem() );
}
void PlaylistDock::titleSort1()
{
	list->sortItems( 0, Qt::AscendingOrder );
	list->scrollToItem( list->currentItem() );
}
void PlaylistDock::titleSort2()
{
	list->sortItems( 0, Qt::DescendingOrder );
	list->scrollToItem( list->currentItem() );
}
void PlaylistDock::collapseAll()
{
	list->collapseAll();
}
void PlaylistDock::expandAll()
{
	list->expandAll();
}
void PlaylistDock::goToPlaying()
{
	if ( list->currentPlaying )
	{
		list->clearSelection();
		list->setCurrentItem( list->currentPlaying );
		list->scrollToItem( list->currentPlaying );
	}
}
void PlaylistDock::queue()
{
	list->queue();
}
void PlaylistDock::findItems( const QString &txt )
{
	QList< QTreeWidgetItem * > itemsToShow = list->findItems( txt, Qt::MatchContains | Qt::MatchRecursive );
	list->processItems( &itemsToShow, !txt.isEmpty() );
	if ( txt.isEmpty() )
	{
		QList< QTreeWidgetItem * > selectedItems = list->selectedItems();
		if ( selectedItems.count() )
			list->scrollToItem( selectedItems[ 0 ] );
		else if ( list->currentPlaying )
			list->scrollToItem( list->currentPlaying );
	}
}
void PlaylistDock::findNext()
{
	bool belowSelection = false;
	QTreeWidgetItem *currentItem = list->currentItem();
	QTreeWidgetItem *firstItem = NULL;
	foreach ( QTreeWidgetItem *tWI, list->getChildren( PlaylistWidget::ALL_CHILDREN ) )
	{
		if ( tWI->isHidden() )
			continue;
		if ( !firstItem && !PlaylistWidget::isGroup( tWI ) )
			firstItem = tWI;
		if ( !belowSelection )
		{
			if ( tWI == currentItem || !currentItem )
				belowSelection = true;
			if ( currentItem )
				continue;
		}
		if ( PlaylistWidget::isGroup( tWI ) )
			continue;
		list->setCurrentItem( tWI );
		return;
	}
	list->setCurrentItem( firstItem );
}
void PlaylistDock::visibleItemsCount( int count )
{
	if ( count < 0 )
		statusL->setText( tr( "Lista jest ładowana..." ) );
	else
		statusL->setText( tr( "Ilość widocznych wpisów" ) + ": " + QString::number( count ) );
}
void PlaylistDock::syncCurrentFolder()
{
	QTreeWidgetItem *tWI = list->currentItem();
	if ( !( tWI && PlaylistWidget::isGroup( tWI ) ) )
		return;
	QString pth = tWI->data( 0, Qt::UserRole ).toString();
	if ( pth.isEmpty() )
		return;
	pth.remove( "file://" );
	if ( pth.right( 1 ) != "/" )
		pth += "/";
	if ( !QFileInfo( pth ).isDir() )
	{
		tWI->setData( 0, Qt::UserRole, QString() );
		tWI->setIcon( 0, *QMPlay2GUI.groupIcon );
		return;
	}
	findE->clear();
	list->sync( pth, tWI );
}
void PlaylistDock::repeat()
{
	const QString name = sender()->objectName();
	const RepeatMode lastRepeatMode = repeatMode;
	if ( name == "normal" )
		repeatMode = Normal;
	else if ( name == "repeatEntry" )
		repeatMode = RepeatEntry;
	else if ( name == "repeatGroup" )
		repeatMode = RepeatGroup;
	else if ( name == "repeatList" )
		repeatMode = RepeatList;
	else if ( name == "random" )
		repeatMode = Random;
	else if ( name == "randomGroup" )
		repeatMode = RandomGroup;
	if ( ( lastRepeatMode == Random && repeatMode != Random ) || ( lastRepeatMode == RandomGroup && repeatMode != RandomGroup ) )
		randomPlayedItems.clear();
}
void PlaylistDock::updateCurrentEntry( const QString &name, int length )
{
	list->updateEntryThr.updateEntry( list->currentPlaying, name, length );
}
