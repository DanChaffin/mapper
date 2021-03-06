/*
 *    Copyright 2012, 2013 Pete Curtis
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENORIENTEERING_FILE_FORMAT_H
#define OPENORIENTEERING_FILE_FORMAT_H

#include <cstddef>
#include <exception>

#include <QtGlobal>
#include <QByteArray>
#include <QFlags>
#include <QList>
#include <QString>
#include <QStringList>

class QIODevice;

class Exporter;
class Importer;
class Map;
class MapView;


/** An exception type thrown by an importer or exporter if it encounters a fatal error.
 */
class FileFormatException : public std::exception  // clazy:exclude=copyable-polymorphic
{
public:
	/** Creates a new exception with the given message
	 *  \param message a text describing the exceptional event that has occurred.
	 */
	FileFormatException(const QString& message = QString());
	
	/** Creates a new exception with the given message
	 *  \param message a text describing the exceptional event that has occurred.
	 */
	FileFormatException(const char* message);
	
	/** Copy-constructor (C++ FAQ 17.17).
	 */
	FileFormatException(const FileFormatException& other) noexcept;
	
	/** Destroys the exception object.
	 */
	~FileFormatException() noexcept override;
	
	/** Returns the message as a QString. 
	 */
	const QString& message() const noexcept;
	
	/** Returns the message as a C string.
	 */
	const char* what() const noexcept override;
	
private:
	QString const msg;
	QByteArray const msg_c;
};


/** Describes a file format understood by this application. Each file format has an ID
 *  (an internal, non-translated string); a description (translated); a file extension
 *  (non-translated); and methods to indicate their support for import or export. Formats
 *  are installed into a FormatRegistry, and can be looked up in a variety of ways.
 *
 *  A typical FileFormat subclass will look like this:
 *
 *  \code
 *  class MyCustomFileFormat : public FileFormat {
 *  public:
 *      MyCustomFileFormat : FileFormat("custom", ImportExport::tr("Custom file"), "custom", true, true) {
 *      }
 *
 *      Importer *createImporter(QIODevice* stream, Map *map, MapView *view) const {
 *          return new CustomImporter(stream, map, view);
 *      }
 *
 *      Exporter *createExporter(QIODevice* stream, Map *map, MapView *view) const {
 *          return new CustomExporter(stream, map, view);
 *      }
 *  }
 *  \endcode
 */
class FileFormat
{
public:
	/** File type enumeration. 
	 * 
	 *  Each file type shall use a distinct bit so that they may be OR-combined.
	 * 
	 *  Currently the program is only used for mapping, and "Map" is the only 
	 *  element. "Course" or "Event" are possible additions.
	 */
	enum FileType
	{
		MapFile     = 0x01,
		OgrFile     = 0x02,     ///< Geospatial vector data supported by OGR
		
		AllFiles    = MapFile,  ///< All types which can be handled by an editor.
		
		EndOfFileTypes
	};
	
	/** A type which handles OR-combinations of file types.
	 */
	Q_DECLARE_FLAGS(FileTypes, FileType)
	
	/** File format features.
	 * 
	 *  Each feature shall use a distinct bit so that they may be OR-combined.
	 */
	enum FormatFeatureFlag
	{
		ImportSupported = 0x01,
		ExportSupported = 0x02,
		ExportLossy     = 0x04,
		
		EndOfFormatFeatures
	};
	
	/** A type which handles OR-combinations of format implementation features.
	 */
	Q_DECLARE_FLAGS(FormatFeatures, FormatFeatureFlag)
	
	/** Creates a new file format with the given parameters.
	 * 
	 *  Don't use a leading dot on the file extension.
	 *  
	 */
	FileFormat(FileType file_type, const char* id, const QString& description, const QString& file_extension, FormatFeatures features);
	
	FileFormat(const FileFormat&) = delete;
	FileFormat(FileFormat&&) = delete;
	
	/** Destroys the file format information. */
	virtual ~FileFormat();
	
	FileFormat& operator=(const FileFormat&) = delete;
	FileFormat& operator=(FileFormat&&) = delete;
	
	/** Registers an alternative file name extension.
	 *  It is used by the filter.
	 */
	void addExtension(const QString& file_extension);
	
	/** Returns the type of file.
	 */
	FileType fileType() const;
	
	/** Returns the internal ID of the file format.
	 */
	const char* id() const;
	
	/** Returns a short human-readable description of the file format.
	 */
	const QString& description() const;
	
	/** Returns the primary file name extension used by this file format.
	 * 
	 *  This shall only be used for constructing new file names. Otherwise
	 *  you will probably need to use fileExtensions().
	 */
	const QString& primaryExtension() const;
	
	/** Returns all file name extension supported by this file format.
	 */
	const QStringList& fileExtensions() const;
	
	/** Returns the filter that represents this format in file dialogs.
	 */
	const QString& filter() const;
	
	/** Returns true if this file format supports importing a map from its associated file type.
	 */
	bool supportsImport() const;
	
	/** Returns true if this file format supports exporting a map to its associated file type.
	 */
	bool supportsExport() const;
	
	/** Returns true if an exporter for this file format is potentially lossy, i.e., if the exported
	 *  file cannot fully represent all aspects of the internal OO map objects. This flag is used by
	 *  the application to warn the user before saving to a lossy file type.
	 */
	bool isExportLossy() const;
	
	/** Returns true if this file format believes it is capable of understanding a file that
	 *  starts with the given byte sequence. "Magic" numbers and version information is commonly
	 *  placed at the beginning of a file, and this method is used by the application to pre-screen
	 *  for a suitable Importer. If there is any doubt about whether the file format can successfully
	 *  process a file, this method should return false.
	 */
	virtual bool understands(const unsigned char *buffer, std::size_t sz) const;
	
	/** Creates an Importer that will read a map file from the given stream into the given map and view.
	 *  The caller can then call doImport() in the returned object to start the import process. The caller
	 *  is responsible for deleting the Importer when it's finished.
	 *
	 *  If the Importer could not be created, then this method should throw a FormatException.
	 */
	virtual Importer* createImporter(QIODevice* stream, Map *map, MapView *view) const;
	
	/** Creates an Exporter that will save the given map and view into the given stream.
	 *  The caller can then call doExport() in the returned object to start the export process. The caller
	 *  is responsible for deleting the Exporter when it's finished.
	 *
	 *  If the Exporter could not be created, then this method should throw a FormatException.
	 */
	virtual Exporter *createExporter(QIODevice* stream, Map *map, MapView *view) const;
	
private:
	FileType file_type;
	const char* format_id;
	QString format_description;
	QStringList file_extensions;
	QString format_filter;
	FormatFeatures format_features;
};


// ### FileFormatException inline and header code ###

inline
FileFormatException::FileFormatException(const QString& message)
 : msg(message)
 , msg_c(message.toLocal8Bit())
{
	// Nothing
}

inline
FileFormatException::FileFormatException(const char* message)
 : msg(QString::fromLatin1(message))
 , msg_c(message)
{
	// Nothing
}

inline
FileFormatException::FileFormatException(const FileFormatException& other) noexcept
 : msg(other.msg)
 , msg_c(other.msg_c)
{
	// Nothing
}

inline
const QString& FileFormatException::message() const noexcept
{
	return msg;
}


// ### FileFormat inline and header code ###

Q_DECLARE_OPERATORS_FOR_FLAGS(FileFormat::FileTypes)

Q_DECLARE_OPERATORS_FOR_FLAGS(FileFormat::FormatFeatures)

inline
FileFormat::FileType FileFormat::fileType() const
{
	return file_type;
}

inline
const char* FileFormat::id() const
{
	return format_id;
}

inline
const QString& FileFormat::description() const
{
	return format_description;
}

inline
const QString& FileFormat::primaryExtension() const
{
	Q_ASSERT(file_extensions.size() > 0); // by constructor
	return file_extensions[0];
}

inline
const QStringList& FileFormat::fileExtensions() const
{
	return file_extensions;
}

inline
const QString& FileFormat::filter() const
{
	return format_filter;
}

inline
bool FileFormat::supportsImport() const
{
	return format_features.testFlag(FileFormat::ImportSupported);
}

inline
bool FileFormat::supportsExport() const
{
	return format_features.testFlag(FileFormat::ExportSupported);
}

inline
bool FileFormat::isExportLossy() const
{
	return format_features.testFlag(FileFormat::ExportLossy);
}


#endif // OPENORIENTEERING_FILE_FORMAT_H
