/***************************************************************************
                          skyobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qregexp.h>
#include "skyobject.h"
#include "ksutils.h"
#include "dms.h"
#include "geolocation.h"

SkyObject::SkyObject() : SkyPoint(0.0, 0.0) {
	Type = 0;
	Magnitude = 0.0;
	MajorAxis = 1.0;
	MinorAxis = 0.0;
	PositionAngle = 0;
	UGC = 0;
	PGC = 0;
	Name = i18n("unnamed object");
	Name2 = "";
	LongName = Name;
	Catalog = "";
  calcCatalogFlags(); // optimize string handling
	Image = 0;
	
}

SkyObject::SkyObject( SkyObject &o ) : SkyPoint( o) {
	Type = o.type();
	Magnitude = o.mag();
	MajorAxis = o.a();
	MinorAxis = o.b();
	PositionAngle = o.pa();
	UGC = o.ugc();
	PGC = o.pgc();
	Name = o.name();
	Name2 = o.name2();
	Catalog = o.catalog();
  calcCatalogFlags(); // optimize string handling
	ImageList = o.ImageList;
	ImageTitle = o.ImageTitle;
	InfoList = o.InfoList;
	InfoTitle = o.InfoTitle;
	Image = o.image();

	setLongName(o.longname());
}

SkyObject::SkyObject( int t, dms r, dms d, double m,
						QString n, QString n2, QString lname, QString cat,
						double a, double b, int pa, int pgc, int ugc ) : SkyPoint( r, d) {
	Type = t;
	Magnitude = m;
	MajorAxis = a;
	MinorAxis = b;
	PositionAngle = pa;
	PGC = pgc;
	UGC = ugc;
	Name = n;
	Name2 = n2;
	Catalog = cat;
  calcCatalogFlags(); // optimize string handling
	Image = 0;

	setLongName(lname);
}

SkyObject::SkyObject( int t, double r, double d, double m,
						QString n, QString n2, QString lname, QString cat,
						double a, double b, int pa, int pgc, int ugc ) : SkyPoint( r, d) {
	Type = t;
	Magnitude = m;
	MajorAxis = a;
	MinorAxis = b;
	PositionAngle = pa;
	PGC = pgc;
	UGC = ugc;
	Name = n;
	Name2 = n2;
	Catalog = cat;
  calcCatalogFlags(); // optimize string handling
	Image = 0;

	setLongName(lname);
}

void SkyObject::setLongName( const QString &longname ) {
	if ( longname.isEmpty() )
		if ( Name.length() )
			LongName = Name;
		else if ( Name2.length() )
			LongName = Name2;
		else
			LongName = i18n( "unnamed object" );
	else
		LongName = longname;
}

QTime SkyObject::riseSetTime( long double jd, const GeoLocation *geo, bool rst ) {

	//this object does not rise or set; return an invalid time
	if ( checkCircumpolar(geo->lat()) )
		return QTime( 25, 0, 0 );  

	QTime UT = riseSetTimeUT( jd, geo->lng(), geo->lat(), rst ); 
	
	//convert UT to LT;
	return UT.addSecs( int( 3600.0*geo->TZ() ) );
}

QTime SkyObject::riseSetTimeUT( long double jd, const dms *gLng, const dms *gLat, bool riseT) {

	QTime UT = auxRiseSetTimeUT(jd, gLng, gLat, ra(), dec(), riseT); 
	// We iterate once more using the calculated UT to compute again 
	// the ra and dec and hence the rise time.
	
	long double jd0 = newJDfromJDandUT( jd, UT );

	SkyPoint sp = getNewCoords(jd, jd0);
	const dms *ram = sp.ra0();
	const dms *decm = sp.dec0();
	
	UT = auxRiseSetTimeUT(jd0, gLng, gLat, ram, decm, riseT); 

	return UT;
}

dms SkyObject::riseSetTimeLST( long double jd, const dms *gLng, const dms *gLat, bool riseT) {
	QTime UT = riseSetTimeUT(jd, gLng, gLat, riseT);
	QDateTime utTime = KSUtils::JDtoUT( jd );
	utTime.setTime( UT );
	return KSUtils::UTtoLST( utTime, gLng ); 
}

QTime SkyObject::auxRiseSetTimeUT( long double jd, const dms *gLng, const dms *gLat, 
			const dms *righta, const dms *decl, bool riseT) {

	// if riseT = true => rise Time, else setTime
	
	dms LST = auxRiseSetTimeLST(gLat, righta, decl, riseT ); 

	//convert LST to Greenwich ST
	dms GST = dms( LST.Degrees() - gLng->Degrees() ).reduce();

	//convert GST to UT
	double T = ( jd - J2000 )/36525.0;
	dms T0, dT, UTh;
	T0.setH( 6.697374558 + (2400.051336*T) + (0.000025862*T*T) );
	T0 = T0.reduce();
	dT.setH( GST.Hours() - T0.Hours() );
	dT = dT.reduce();

	UTh.setH( 0.9972695663 * dT.Hours() );
	UTh = UTh.reduce();
	
	
	return QTime( UTh.hour(), UTh.minute(), UTh.second() );
}

dms SkyObject::auxRiseSetTimeLST( const dms *gLat, const dms *righta, const dms *decl, bool riseT ) {

//	double r = -1.0 * tan( gLat.radians() ) * tan( decl.radians() );
//	double H = acos( r )*180./acos(-1.0); //180/Pi converts radians to degrees
	dms h0 = elevationCorrection();
	double H = approxHourAngle (&h0, gLat, decl);
	
	dms LST;
	
	// rise Time or setTime

	if (riseT) 
		LST.setH( 24.0 + righta->Hours() - H/15.0 );
	else
		LST.setH( righta->Hours() + H/15.0 );

	LST = LST.reduce();

	return LST;
}


dms SkyObject::riseSetTimeAz(long double jd, const GeoLocation *geo, bool riseT) {

	dms Azimuth;
	double AltRad, AzRad;
	double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
	double sinAlt, cosAlt;

	QTime UT = riseSetTimeUT( jd, geo->lng(), geo->lat(), riseT );
	long double jd0 = newJDfromJDandUT(jd, UT);
	SkyPoint sp = getNewCoords(jd,jd0);
	const dms *ram = sp.ra0();
	const dms *decm = sp.dec0();

	dms LST = auxRiseSetTimeLST( geo->lat(), ram, decm, riseT );
	dms HourAngle = dms( LST.Degrees() - ram->Degrees() );

	geo->lat()->SinCos( sinlat, coslat );
	dec()->SinCos( sindec, cosdec );
	HourAngle.SinCos( sinHA, cosHA );

	sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
	AltRad = asin( sinAlt );
	cosAlt = cos( AltRad );

	AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
	// AzRad = acos( sindec /( coslat*cosAlt ) );
	if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity
	Azimuth.setRadians( AzRad );

	return Azimuth;
}

QTime SkyObject::transitTimeUT(long double jd, const dms *gLng ) {

	QDateTime utDateTime = KSUtils::JDtoUT( jd );
	dms LST = KSUtils::UTtoLST( utDateTime, gLng );
	
	//dSec is the number of seconds until the object transits.
	dms HourAngle = dms ( LST.Degrees() - ra()->Degrees() );
	int dSec = int( -3600.*HourAngle.Hours() );
	
//UT is now a QTime
//	dms UT0 = QTimeToDMS( utDateTime.time() );
//	UT0 = dms (UT0.Degrees() + dSec*15/3600.);

	//UT0 is the first guess at the transit time.  
	QTime UT0 = utDateTime.time().addSecs( dSec );
	long double jd0 = newJDfromJDandUT(jd, UT0);

	//recompute object's position at UT0 and then find 
	//transit time of this refined position
	SkyPoint sp = getNewCoords(jd, jd0);
	const dms *ram = sp.ra0();

	HourAngle = dms ( LST.Degrees() - ram->Degrees() );
	dSec = int( -3600.*HourAngle.Hours() );

	return utDateTime.time().addSecs( dSec );
}

QTime SkyObject::transitTime( long double jd, const GeoLocation *geo ) {
	QTime UT = transitTimeUT(jd, geo->lng() );
	return UT.addSecs( int( 3600.*geo->TZ() ) );
}

dms SkyObject::transitAltitude(long double jd, const GeoLocation *geo) {
	QTime UT = transitTimeUT(jd, geo->lng());
	long double jd0 = newJDfromJDandUT(jd, UT);
	SkyPoint sp = getNewCoords(jd,jd0);
	const dms *decm = sp.dec0();
	
	dms delta;
	delta.setRadians( asin ( sin (geo->lat()->radians()) * 
				sin ( decm->radians() ) +
				cos (geo->lat()->radians()) * 
				cos (decm->radians() ) ) );

	return delta;
}

//This function is no longer used
/*
dms SkyObject::gstAtZeroUT (long double jd) {

	long double jd0 = KSUtils::JDatZeroUT (jd) ;
	long double s = jd0 - 2451545.0;
	double t = s/36525.0;
	dms T0;
	T0.setH (6.697374558 + 2400.051336*t + 0.000025862*t*t + 
			0.000000002*t*t*t);

	return T0;
}
*/

double SkyObject::approxHourAngle ( const dms *h0, const dms *gLat, const dms *dec ) {

	double sh0 = - sin ( h0->radians() );
	double r = (sh0 - sin( gLat->radians() ) * sin(dec->radians() ))
		 / (cos( gLat->radians() ) * cos( dec->radians() ) );

	double H = acos( r )/dms::DegToRad;

	return H;
}

dms SkyObject::elevationCorrection(void) {
	if ( name() == "Sun"  )
		return dms(0.5667);
	else if ( name() == "Moon" )
		return dms(0.125); // a rough approximation
	else
		return dms(0.8333);
}

long double SkyObject::newJDfromJDandUT(long double jd, QTime UT) {
	QDateTime dt = KSUtils::JDtoUT(jd);
	dt.setTime( UT );
	return KSUtils::UTtoJD( dt );
}

SkyPoint SkyObject::getNewCoords(long double jd, long double jd0) {

	// we save the original state of the object
	KSNumbers *num = new KSNumbers(jd);

	KSNumbers *num1 = new KSNumbers(jd0);
	updateCoords(num1);
	const dms *ram = ra();
	const dms *decm = dec();
	delete num1;

	// we leave the object in its original state
	updateCoords(num);
	delete num;

	return SkyPoint(ram, decm);
}

bool SkyObject::checkCircumpolar( const dms *gLat ) {
	double r = -1.0 * tan( gLat->radians() ) * tan( dec()->radians() );
	if ( r < -1.0 || r > 1.0 )
		return true;
	else
		return false;
}

dms SkyObject::QTimeToDMS(QTime qtime) {

	dms tt;
	tt.setH(qtime.hour(), qtime.minute(), qtime.second());
	tt.reduce();

	return tt;
}

float SkyObject::e( void ) const {
	if ( MajorAxis==0.0 || MinorAxis==0.0 ) return 1.0; //assume circular
	return MinorAxis / MajorAxis;
}

QImage* SkyObject::readImage( void ) {
	QFile file;
	if ( Image==0 ) { //Image not currently set; try to load it from disk.
		QString fname = Name.lower().replace( QRegExp(" "), "" ) + ".png";

		if ( KSUtils::openDataFile( file, fname ) ) {
			file.close();
			Image = new QImage( file.name(), "PNG" );
		}
	}

	return Image;
}

QString SkyObject::typeName( void ) const {
        if ( Type==0 ) return i18n( "Star" );
	else if ( Type==1 ) return i18n( "Catalog Star" );
	else if ( Type==2 ) return i18n( "Planet" );
	else if ( Type==3 ) return i18n( "Open Cluster" );
	else if ( Type==4 ) return i18n( "Globular Cluster" );
	else if ( Type==5 ) return i18n( "Gaseous Nebula" );
	else if ( Type==6 ) return i18n( "Planetary Nebula" );
	else if ( Type==7 ) return i18n( "Supernova Remnant" );
	else if ( Type==8 ) return i18n( "Galaxy" );
	else if ( Type==9 ) return i18n( "Comet" );
	else if ( Type==10 ) return i18n( "Asteroid" );
	else return i18n( "Unknown Type" );
}

// optimizate string handling
void SkyObject::calcCatalogFlags() {
  bIsCatalogIC   = (Catalog == "IC" );
  bIsCatalogM    = (Catalog == "M" );
  bIsCatalogNGC  = (Catalog == "NGC" );
  bIsCatalogNone = (Catalog.isEmpty() );
}

