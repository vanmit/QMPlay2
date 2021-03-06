#include <Main.hpp>

#include <MainWidget.hpp>
#include <PlayClass.hpp>
#include <Functions.hpp>
#include <VideoDock.hpp>
#include <Settings.hpp>
#include <Module.hpp>

#include <QDesktopWidget>
#include <QStyleFactory>
#include <QStyleOption>
#include <QLocalServer>
#include <QLocalSocket>
#include <QApplication>
#include <QImageReader>
#include <QMessageBox>
#include <QFileDialog>
#include <QTranslator>
#include <QPainter>
#include <QLocale>
#include <QBuffer>
#include <QFile>
#include <QDir>
#if QT_VERSION < 0x050000
	#include <QTextCodec>
#endif

#include <time.h>

static QTranslator translator;
static bool useGui = true;

QMPlay2GUIClass &QMPlay2GUIClass::instance()
{
	static QMPlay2GUIClass singleton;
	return singleton;
}

QString QMPlay2GUIClass::getLongFromShortLanguage( const QString &lng )
{
	const QString lang = QLocale::languageToString( QLocale( lng ).language() );
	return lang == "C" ? lng : lang;
}
QStringList QMPlay2GUIClass::getModules( const QString &type, int typeLen )
{
	QStringList defaultModules;
#if defined Q_OS_LINUX
	if ( type == "videoWriters" )
		defaultModules << "XVideo Writer" << "OpenGL Writer";
	else if ( type == "audioWriters" )
		defaultModules << "PulseAudio Writer" << "ALSA Writer";
#elif defined Q_OS_WIN
	if ( type == "videoWriters" )
	{
		if ( QSysInfo::windowsVersion() < QSysInfo::WV_6_0 ) //dla starszych systemów od Visty
			defaultModules << "DirectDraw Writer";
		else
			defaultModules << "OpenGL Writer";
	}
#endif
	QStringList availableModules;
	const QString moduleType = type.mid( 0, typeLen );
	foreach ( Module *module, QMPlay2Core.getPluginsInstance() )
		foreach ( Module::Info moduleInfo, module->getModulesInfo() )
			if ( ( moduleInfo.type == Module::WRITER && moduleInfo.extensions.contains( moduleType ) ) || ( moduleInfo.type == Module::DECODER && moduleType == "decoder" ) )
				availableModules += moduleInfo.name;
	QStringList modules;
	foreach ( QString module, QMPlay2GUI.settings->get( type, defaultModules ).toStringList() )
	{
		const int idx = availableModules.indexOf( module );
		if ( idx > -1 )
		{
			availableModules.removeAt( idx );
			modules += module;
		}
	}
	return modules + availableModules;
}
QString QMPlay2GUIClass::getPipe()
{
#ifdef Q_OS_WIN
	return "\\\\.\\pipe\\QMPlay2";
#else
	return QDir::tempPath() + "/QMPlay2." + QString( getenv( "USER" ) );
#endif
}

void QMPlay2GUIClass::saveCover( QByteArray cover )
{
	QBuffer buffer( &cover );
	if ( buffer.open( QBuffer::ReadOnly ) )
	{
		const QString fileExtension = QImageReader::imageFormat( &buffer );
		buffer.close();
		if ( !fileExtension.isEmpty() )
		{
			QString fileName = QFileDialog::getSaveFileName( QMPlay2GUI.mainW, tr( "Zapis okładki" ), QMPlay2GUI.getCurrentPth(), fileExtension.toUpper() + " (*." + fileExtension + ')' );
			if ( !fileName.isEmpty() )
			{
				if ( !fileName.endsWith( '.' + fileExtension ) )
					fileName += '.' + fileExtension;
				QFile f( fileName );
				if ( f.open( QFile::WriteOnly ) )
				{
					f.write( cover );
					QMPlay2GUI.setCurrentPth( Functions::filePath( fileName ) );
				}
			}
		}
	}
}

void QMPlay2GUIClass::drawPixmap( QPainter &p, QWidget *w, QPixmap pixmap )
{
	if ( pixmap.width() > w->width() || pixmap.height() > w->height() )
		pixmap = pixmap.scaled( w->width(), w->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
	if ( !w->isEnabled() )
	{
		QStyleOption opt;
		opt.initFrom( w );
		pixmap = w->style()->generatedIconPixmap( QIcon::Disabled, pixmap, &opt );
	}
	p.drawPixmap( w->width() / 2 - pixmap.width() / 2, w->height() / 2 - pixmap.height() / 2, pixmap );
}

void QMPlay2GUIClass::runUpdate( const QString &UpdateFile )
{
	settings->set( "UpdateFile", "remove:" + UpdateFile );
	QMPlay2Core.run( UpdateFile, "--Auto" );
}

QStringList QMPlay2GUIClass::getLanguages()
{
	QStringList langs = QDir( langPath ).entryList( QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks );
	for ( int i = 0 ; i < langs.size() ; i++ )
	{
		int idx = langs[ i ].indexOf( '.' );
		if ( idx > 0 )
			langs[ i ].remove( idx, langs[ i ].size() - idx );
	}
	return langs;
}
void QMPlay2GUIClass::setLanguage()
{
	QString systemLang = QLocale::system().name();
	const int idx = systemLang.indexOf( '_' );
	if ( idx > -1 )
		systemLang.remove( idx, systemLang.size() - idx );
	lang = settings->get( "Language", systemLang ).toString();
	if ( lang.isEmpty() )
		lang = systemLang;
	if ( !translator.load( lang, langPath ) && lang != "pl" && lang != "en" && translator.load( "en", langPath ) )
		lang = "en";
}

void QMPlay2GUIClass::setStyle()
{
	QString defaultStyle;
#ifdef Q_OS_WIN
	if ( QSysInfo::windowsVersion() < QSysInfo::WV_6_0 )
	{
		QStringList styles = QStyleFactory::keys();
		if ( styles.contains( "qtcurve", Qt::CaseInsensitive ) )
			defaultStyle = "qtcurve";
		else if ( styles.contains( "cleanlooks", Qt::CaseInsensitive ) )
			defaultStyle = "cleanlooks";
		else if ( styles.contains( "plastique", Qt::CaseInsensitive ) )
			defaultStyle = "plastique";
		else if ( styles.contains( "fusion", Qt::CaseInsensitive ) )
			defaultStyle = "fusion";
	}
#endif
	qApp->setStyle( settings->get( "Style", defaultStyle ).toString() );
}
void QMPlay2GUIClass::loadIcons()
{
	deleteIcons();
	groupIcon = new QIcon( getIconFromTheme( "folder-orange" ) );
	mediaIcon = new QIcon( getIconFromTheme( "applications-multimedia" ) );
	folderIcon = new QIcon( getIconFromTheme( "folder" ) );
}

QString QMPlay2GUIClass::getCurrentPth( QString pth, bool leaveFilename )
{
	if ( pth.left( 7 ) == "file://" )
		pth.remove( 0, 7 );
	if ( !leaveFilename )
		pth = Functions::filePath( pth );
	if ( !QFileInfo( pth ).exists() )
		pth = settings->get( "currPth" ).toString();
	return pth;
}
void QMPlay2GUIClass::setCurrentPth( const QString &pth )
{
	settings->set( "currPth", Functions::cleanPath( pth ) );
}

void QMPlay2GUIClass::restoreGeometry( const QString &pth, QWidget *w, const QSize &def_size )
{
	QRect geo = settings->get( pth ).toRect();
	if ( geo.isValid() )
		w->setGeometry( geo );
	else
	{
		w->move( qApp->desktop()->width()/2 - def_size.width()/2, qApp->desktop()->height()/2 - def_size.height()/2 );
		w->resize( def_size );
	}
}

void QMPlay2GUIClass::updateInDockW()
{
	VideoDock *videoDock = qobject_cast< MainWidget * >( mainW )->videoDock;
	if ( videoDock )
		videoDock->updateIDW();
}

QMPlay2GUIClass::QMPlay2GUIClass() :
	groupIcon( NULL ), mediaIcon( NULL ), folderIcon( NULL ),
#ifdef Q_OS_WIN
	forbid_screensaver( false ),
#endif
	mainW( NULL )
{
	qmp2Pixmap = useGui ? new QPixmap( ":/QMPlay2" ) : NULL;
}
QMPlay2GUIClass::~QMPlay2GUIClass()
{
	if ( useGui )
	{
		delete qmp2Pixmap;
		deleteIcons();
	}
}

void QMPlay2GUIClass::deleteIcons()
{
	delete groupIcon;
	delete mediaIcon;
	delete folderIcon;
	groupIcon = mediaIcon = folderIcon = NULL;
}

/**/

static QPair< QStringList, QStringList > QMPArguments;

static void parseArguments( QStringList &arguments )
{
	QString param;
	while ( arguments.count() )
	{
		QString arg = arguments.takeAt( 0 );
		if ( arg.left( 1 ) == "-" )
		{
			param = arg;
			while ( param[ 0 ] == '-' )
				param.remove( 0, 1 );
			if ( !param.isEmpty() && !QMPArguments.first.contains( param ) )
			{
				QMPArguments.first += param;
				QMPArguments.second += "";
			}
			else
				param.clear();
		}
		else if ( !param.isEmpty() )
		{
			QString &data = QMPArguments.second[ QMPArguments.second.count() - 1 ];
			if ( !data.isEmpty() )
				data += " ";
			data += arg;
		}
		else if ( !QMPArguments.first.contains( "open" ) )
		{
			param = "open";
			QMPArguments.first += param;
			QMPArguments.second += arg;
		}
	}
}
static int showHelp( const QByteArray &ver )
{
	QFile f;
	f.open( stdout, QFile::WriteOnly );
	f.write( "QMPlay2 - QT Media Player 2 (" + ver + ")\n" );
	f.write( QObject::tr(
"  Lista parametrów:\n"
"    -open      \"adres\"\n"
"    -enqueue   \"adres\"\n"
"    -noplay     - nie odtwarza muzyki po uruchomieniu (omija opcję \"Zapamiętaj pozycję odtwarzania\")\n"
"    -toggle     - przełącza play/pause\n"
"    -show       - zapewnia, że okno będzie widoczne, jeżeli aplikacja jest uruchomiona\n"
"    -fullscreen - przełącza z/do pełnego ekranu\n"
"    -volume     - ustawia głośność [0..100]\n"
"    -speed      - ustawia szybkość odtwarzania [0.05..100.0]\n"
"    -seek       - przewija do podanej wartości [s]\n"
"    -stop       - zatrzymuje odtwarzanie\n"
"    -next       - odtwarza następny na liście\n"
"    -prev       - odtwarza poprzedni na liście\n"
"    -quit       - zakańcza działanie aplikacji"
	).toUtf8() + "\n" );
	return 0;
}
static bool writeToSocket( QLocalSocket &socket )
{
	bool ret = false;
	for ( int i = QMPArguments.first.count() - 1 ; i >= 0 ; i-- )
	{
		if ( QMPArguments.first[ i ] == "noplay" )
			continue;
		else if ( QMPArguments.first[ i ] == "open" || QMPArguments.first[ i ] == "enqueue" )
		{
			if ( !QMPArguments.second[ i ].isEmpty() )
				QMPArguments.second[ i ] = Functions::Url( QMPArguments.second[ i ] );
#ifdef Q_OS_WIN
			if ( QMPArguments.second[ i ].left( 7 ) == "file://" )
				QMPArguments.second[ i ].remove( 0, 7 );
#endif
		}
		socket.write( QString( QMPArguments.first[ i ] + '\t' + QMPArguments.second[ i ] ).toUtf8() + '\0' );
		ret = true;
	}
	return ret;
}

#if QT_VERSION >= 0x050000 && !defined Q_OS_WIN
	#define QT5_NOT_WIN
	#include <setjmp.h>
	static jmp_buf env;
	static bool qAppOK;
#endif
#include <signal.h>
static void signal_handler( int s )
{
#ifdef Q_OS_WIN
	const int SC = SIGBREAK;
#else
	const int SC = SIGKILL;
#endif
	switch ( s )
	{
#ifndef Q_WS_WIN
		case SIGTSTP:
			raise( SC );
			break;
#endif
		case SIGINT:
		{
			QWidget *modalW = qApp->activeModalWidget();
			if ( !modalW && QMPlay2GUI.mainW )
			{
				QMPlay2GUI.mainW->close();
				QMPlay2GUI.mainW = NULL;
			}
			else
				raise( SC );
		} break;
		case SIGSEGV:
			QMPlay2Core.logError( "Program się wysypał (SIGSEGV)" );
#ifndef Q_OS_WIN
			raise( SC );
#endif
			break;
		case SIGABRT:
#ifdef QT5_NOT_WIN
			if ( !qAppOK && useGui )
			{
				useGui = false;
				longjmp( env, 1 );
			}
#endif
			QMPlay2Core.logError( "Program przerwał działanie (SIGABRT)" );
#ifndef Q_OS_WIN
			raise( SC );
#endif
			break;
		case SIGFPE:
			QMPlay2Core.logError( "Program się wysypał (SIGFPE)" );
#ifndef Q_OS_WIN
			raise( SC );
#endif
			break;
	}
}


int main( int argc, char *argv[] )
{
#ifndef Q_OS_WIN
	const QString unixPath = "/../share/qmplay2";
#else
	const QString unixPath;
#endif

	signal( SIGINT, signal_handler );
#ifndef Q_WS_WIN
	signal( SIGTSTP, signal_handler );
#endif
	signal( SIGFPE, signal_handler );
	signal( SIGSEGV, signal_handler );
	signal( SIGABRT, signal_handler );
#ifdef Q_WS_X11
	useGui = getenv( "DISPLAY" );
#endif
#ifdef QT5_NOT_WIN
	if ( !setjmp( env ) )
#endif
		new QApplication( argc, argv, useGui );
#ifdef QT5_NOT_WIN
	qAppOK = true;
#endif
#if QT_VERSION < 0x050000
	QTextCodec::setCodecForTr( QTextCodec::codecForName( "UTF-8" ) );
	QTextCodec::setCodecForCStrings( QTextCodec::codecForName( "UTF-8" ) );
#endif
	qApp->setApplicationName( "QMPlay2" );

	QStringList arguments = qApp->arguments();
	arguments.removeAt( 0 );
	const bool help = arguments.contains( "--help" ) || arguments.contains( "-h" );
	if ( !help )
	{
		parseArguments( arguments );

		QLocalSocket socket;
		socket.connectToServer( QMPlay2GUI.getPipe(), QIODevice::WriteOnly );
		if ( socket.waitForConnected( 1000 ) )
		{
			if ( writeToSocket( socket ) )
			{
				socket.waitForBytesWritten( 1000 );
				useGui = false;
			}
			socket.disconnectFromServer();
		}
#ifndef Q_OS_WIN
		else if ( QFile::exists( QMPlay2GUI.getPipe() ) )
		{
			QFile::remove( QMPlay2GUI.getPipe() );
			QMPArguments.first += "noplay";
			QMPArguments.second += QString();
		}
#endif
		if ( !useGui )
			return 0;
	}

	qApp->installTranslator( &translator );
	qApp->setQuitOnLastWindowClosed( false );
	QMPlay2GUI.langPath = qApp->applicationDirPath() + unixPath + "/lang/";

	qsrand( time( NULL ) );

	do
	{
		/* QMPlay2GUI musi być wywołane już wcześniej */
		QMPlay2Core.init( !help, qApp->applicationDirPath() + unixPath );

		Settings &settings = QMPlay2Core.getSettings();
		QString ver = settings.getString( "Version", QMPlay2Version );
		settings.set( "Version", QMPlay2Version );
		settings.set( "LastQMPlay2Path", qApp->applicationDirPath() );

		QMPlay2GUI.setLanguage();

		if ( help )
			return showHelp( ver.toUtf8() );

		QMPlay2GUI.loadIcons();

		QMPlay2GUI.pipe = new QLocalServer;
#ifndef Q_OS_WIN
		if ( !QMPlay2GUI.pipe->listen( QMPlay2GUI.getPipe() ) )
#else
		if ( WaitNamedPipeA( QMPlay2GUI.getPipe().toLatin1(), NMPWAIT_USE_DEFAULT_WAIT ) || !QMPlay2GUI.pipe->listen( QMPlay2GUI.getPipe() ) )
#endif
		{
			delete QMPlay2GUI.pipe;
			QMPlay2GUI.pipe = NULL;
			if ( settings.getBool( "AllowOnlyOneInstance" ) )
			{
				QMPlay2Core.quit();
				QLocalSocket socket;
				socket.connectToServer( QMPlay2GUI.getPipe(), QIODevice::WriteOnly );
				if ( socket.waitForConnected( 1000 ) )
				{
					socket.write( QByteArray( "show\t" ) + '\0' );
					socket.waitForBytesWritten( 1000 );
					socket.disconnectFromServer();
				}
				break;
			}
		}

		qApp->setWindowIcon( QMPlay2Core.getQMPlay2Pixmap() );
		QMPlay2GUI.setStyle();

		QString UpdateFile = settings.getString( "UpdateFile" );
		if ( UpdateFile.left( 7 ) == "remove:" )
		{
			UpdateFile.remove( 0, 7 );
			if ( ver != QMPlay2Version )
			{
				QString updateString = QObject::tr( "QMPlay2 został zaktualizowany do wersji" ) + " " + QMPlay2Version;
				QMPlay2Core.logInfo( updateString );
				QMessageBox::information( NULL, qApp->applicationName(), updateString );
			}
			QFile::remove( UpdateFile );
			settings.remove( "UpdateFile" );
		}
		else if ( QFileInfo( UpdateFile ).size() )
		{
			QMPlay2GUI.runUpdate( UpdateFile );
			QMPlay2Core.quit();
			break;
		}
		UpdateFile.clear();
		ver.clear();

		QMPlay2GUI.restartApp = QMPlay2GUI.removeSettings = false;
		new MainWidget( QMPArguments );
		qApp->exec();
		QMPlay2GUI.mainW = NULL;

		QString settingsDir = QMPlay2Core.getSettingsDir();
		QMPlay2Core.quit();
		if ( QMPlay2GUI.removeSettings )
			foreach ( QString fName, QDir( settingsDir ).entryList( QStringList( "*.ini" ) ) )
				QFile::remove( settingsDir + fName );

		delete QMPlay2GUI.pipe;
	} while ( QMPlay2GUI.restartApp );

	delete qApp;
	return 0;
}
