#include <AddressBox.hpp>

#include <QMPlay2Extensions.hpp>
#include <Functions.hpp>
#include <Module.hpp>
#include <Main.hpp>

#include <QGridLayout>

AddressBox::AddressBox( Qt::Orientation o, QString url )
{
	pB.addItem( QMPlay2Core.getQMPlay2Pixmap(), tr( "Bezpośredni adres" ), DIRECT );

	foreach ( Module *module, QMPlay2Core.getPluginsInstance() )
		foreach ( Module::Info mod, module->getModulesInfo() )
			if ( mod.type == Module::DEMUXER && !mod.name.contains( ' ' ) )
			{
				QIcon icon;
				if ( !mod.img.isNull() )
					icon = QPixmap::fromImage( mod.img );
				else if ( !module->image().isNull() )
					icon = QPixmap::fromImage( module->image() );
				else
					icon = QMPlay2Core.getQMPlay2Pixmap();
				pB.addItem( icon, mod.name, DEMUXER );
			}

	foreach ( QMPlay2Extensions *QMPlay2Ext, QMPlay2Extensions::QMPlay2ExtensionsList() )
		foreach ( QMPlay2Extensions::AddressPrefix addressPrefix, QMPlay2Ext->addressPrefixList() )
			pB.addItem( QPixmap::fromImage( addressPrefix.img ), addressPrefix, GUI_EXT );

	if ( !url.isEmpty() )
	{
		QString prefix, param;
		if ( Functions::splitPrefixAndUrlIfHasPluginPrefix( url, &prefix, &url, &param ) )
		{
			pB.setCurrentIndex( pB.findText( prefix ) );
			pE.setText( param );
		}
		else
		{
			QString scheme = Functions::getUrlScheme( url );
			int idx = pB.findText( scheme );
			if ( idx > -1 )
			{
				pB.setCurrentIndex( idx );
				url.remove( scheme + "://" );
			}
		}
		aE.setText( url );
	}

	connect( &pB, SIGNAL( currentIndexChanged( int ) ), this, SLOT( pBIdxChanged() ) );
	connect( &aE, SIGNAL( textEdited( const QString & ) ), this, SLOT( aETextChanged() ) );

	pE.setToolTip( tr( "Dodatkowy parametr dla plugina" ) );
	pE.setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );

	QGridLayout *layout = new QGridLayout( this );
	layout->setContentsMargins( 0, 0, 0, 0 );
	switch ( o )
	{
		case Qt::Horizontal:
			layout->addWidget( &pB, 0, 0 );
			layout->addWidget( &aE, 0, 1 );
			layout->addWidget( &pE, 0, 2 );
			break;
		case Qt::Vertical:
			layout->addWidget( &pB, 0, 0, 1, 2 );
			layout->addWidget( &aE, 1, 0, 1, 1 );
			layout->addWidget( &pE, 1, 1, 1, 1 );
			break;
	}

	pBIdxChanged();
}

QString AddressBox::url() const
{
	switch ( currentPrefixType() )
	{
		case AddressBox::DIRECT:
			return aE.text();
		case AddressBox::DEMUXER:
			return pB.currentText() + "://" + aE.text();
		case AddressBox::GUI_EXT:
			return pB.currentText() + "://{" + aE.text() + "}" + pE.text();
	}
	return QString();
}

void AddressBox::pBIdxChanged()
{
	pE.setVisible( currentPrefixType() == GUI_EXT );
	emit directAddressChanged();
}
void AddressBox::aETextChanged()
{
	emit directAddressChanged();
}
