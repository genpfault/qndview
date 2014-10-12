#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include <wx/wfstream.h>
#include <wx/gifdecod.h>
#include <wx/anidecod.h>
#include <wx/image.h>
#include <wx/cmdline.h>

#include <wx/dir.h>
#include <wx/filename.h>

#include "ImagePanel.h"

using namespace std;

// breaks an animation into a sequence of frames
// todo: re-write without using GUI objects (wxMemoryDC, wxBitmap)
vector< AnimationFrame > LoadAnimation( wxAnimationDecoder& ad, wxInputStream& stream )
{
    vector< AnimationFrame > frames;

    if( !ad.Load( stream ) )
        return frames;

    wxBitmap frame( ad.GetAnimationSize() );
    wxMemoryDC dc( frame );
    dc.SetBackground( wxBrush( ad.GetBackgroundColour() ) );
    dc.Clear();

    frames.resize( ad.GetFrameCount() );
    for( unsigned int i = 0; i < frames.size(); ++i )
    {
        const wxRect frameRect( ad.GetFramePosition( i ), ad.GetFrameSize( i ) );

        wxBitmap prvBitmap;
        if( wxANIM_TOPREVIOUS == ad.GetDisposalMethod( i ) )
        {
            dc.SelectObject( wxNullBitmap );
            prvBitmap = frame.GetSubBitmap( frameRect );
            dc.SelectObject( frame );
        }

        wxImage img;
        ad.ConvertToImage( i, &img );
        dc.DrawBitmap( wxBitmap( img ), frameRect.GetPosition(), true );

        dc.SelectObject( wxNullBitmap );
        frames[ i ].mImage = new wxImage( frame.ConvertToImage() );
        frames[ i ].mDelay = static_cast< unsigned int >( ad.GetDelay( i ) );
        dc.SelectObject( frame );

        switch( ad.GetDisposalMethod( i ) )
        {
            case wxANIM_DONOTREMOVE:
            case wxANIM_UNSPECIFIED:
                break;
            case wxANIM_TOBACKGROUND:
                dc.SetBrush( wxBrush( ad.GetBackgroundColour() ) );
                dc.SetPen( *wxTRANSPARENT_PEN );
                dc.DrawRectangle( frameRect );
                break;
            case wxANIM_TOPREVIOUS:
                dc.DrawBitmap( prvBitmap, frameRect.GetPosition(), true );
                break;
            default:
                break;
        }
    }

    return frames;
}


// load a (possibly multi-frame) image from a stream
vector< AnimationFrame > LoadImage( wxInputStream& stream )
{
    if( !stream.IsOk() )
    {
        return vector< AnimationFrame >();
    }

    // special-case animations
    if( wxGIFDecoder().CanRead( stream ) )
    {
        wxGIFDecoder decoder;
        return LoadAnimation( decoder, stream );
    }
    if( wxANIDecoder().CanRead( stream ) )
    {
        wxANIDecoder decoder;
        return LoadAnimation( decoder, stream );
    }

    // generic multi-image loading
    vector< AnimationFrame > frames( wxImage::GetImageCount( stream ) );
    for( int i = 0; i < static_cast< int >( frames.size() ); ++i )
    {
        wxSharedPtr< wxImage > image( new wxImage );
        bool success = false;
        {
            // bug workaround
            // http://trac.wxwidgets.org/ticket/15331
            wxLogNull logNo;
            success = image->LoadFile( stream, wxBITMAP_TYPE_ANY, i );
        }

        frames[ i ].mImage = image;
        frames[ i ].mDelay = -1;
    }
    return frames;
}


// slurp filenames from a directory traversal into a list of wxFileNames
class FileNameTraverser : public wxDirTraverser
{
public:
    typedef std::list< wxFileName > FileList;
    FileNameTraverser( FileList& files ) : mFiles( files ) {}

    virtual wxDirTraverseResult OnFile( const wxString& filename )
    {
        mFiles.push_back( wxFileName( filename ) );
        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir( const wxString& WXUNUSED( dirname ) )
    {
        return wxDIR_CONTINUE;
    }

private:
    FileList& mFiles;
};


// Define a new frame type: this is going to be our main frame
class MyFrame : public wxFrame
{
public:
    MyFrame( const wxString& title, const wxString& initialPath )
        : wxFrame( NULL, wxID_ANY, title )
        , mImagePanel( new wxImagePanel( this ) )
    {
        // query all active handlers for their supported extension(s)
        std::set< wxString > exts;
        for( const auto obj : wxImage::GetHandlers() )
        {
            const auto handler = dynamic_cast< const wxImageHandler* >( obj );
            exts.insert( handler->GetExtension() );
            for( const auto& ext : handler->GetAltExtensions() )
            {
                exts.insert( ext );
            }
        }

        wxFileName initialFileName;
        if( wxDirExists( initialPath ) )
            initialFileName.AssignDir( initialPath );
        else
            initialFileName.Assign( initialPath );

        wxDir dir( initialFileName.GetPath() );
        FileNameTraverser trav( mFiles );
        dir.Traverse( trav, "", wxDIR_FILES );

        // remove filenames whose extensions we don't support
        mFiles.remove_if( [&]( const wxFileName& filename )
        { 
            return exts.end() == exts.find( filename.GetExt() );
        } );

        mCurFile = mFiles.begin();
        if( !initialFileName.GetName().empty() )
        {
            // try to find the initial file in the file list
            while( mCurFile != mFiles.end() && 
                   mCurFile->GetName() != initialFileName.GetName() )
            {
                mCurFile++;
            }

            if( mFiles.end() == mCurFile )
            {
                // probably shouldn't happen
                mCurFile = mFiles.begin();
            }
        }

        // create a menu bar
        wxMenu *fileMenu = new wxMenu;

        // the "About" item should be in the help menu
        wxMenu *helpMenu = new wxMenu;
        helpMenu->Append(wxID_ABOUT, "&About\tF1", "Show about dialog");
        Bind( wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT );

        fileMenu->Append(wxID_EXIT, "E&xit\tAlt-X", "Quit this program");
        Bind( wxEVT_MENU, &MyFrame::OnQuit, this, wxID_EXIT );

        // now append the freshly created menu to the menu bar...
        wxMenuBar *menuBar = new wxMenuBar();
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(helpMenu, "&Help");

        // ... and attach this menu bar to the frame
        SetMenuBar(menuBar);

        // create a status bar just for fun (by default with 1 pane only)
        CreateStatusBar(2);
        SetStatusText("Welcome to wxWidgets!");

        LoadCurrentFile();

        // let our frame get first crack at keyboard events
        // so we can handle things like fullscreen toggle
        // and changing to the next/previous image
        mImagePanel->Bind( wxEVT_KEY_UP, &MyFrame::OnKeyUp, this );
    }

    void LoadCurrentFile()
    {
        if( mFiles.end() != mCurFile && mCurFile->Exists() )
        {
            SetTitle( mCurFile->GetFullName() + " - QndView" );

            wxFileStream fs( mCurFile->GetFullPath() );
            vector< AnimationFrame > frames( LoadImage( fs ) );
            mImagePanel->SetImages( frames );
        }
    }

    void AdvanceFile( bool forward = true )
    {
        if( forward )
        {
            mCurFile++;
            if( mFiles.end() == mCurFile )
                mCurFile = mFiles.begin();
        }
        else
        {
            if( mFiles.begin() == mCurFile )
                mCurFile = mFiles.end();
            mCurFile--;
        }

        LoadCurrentFile();
    }

    void OnKeyUp( wxKeyEvent& event )
    {
        switch( event.GetKeyCode() )
        {
            case 'F':
                // toggle fullscreen
                ShowFullScreen( !IsFullScreen() );
                break;
            case WXK_PAGEUP:
                AdvanceFile( false );
                break;
            case WXK_PAGEDOWN:
                AdvanceFile( true );
                break;
            default:
                // we didn't handle this event so let downstream handlers try
                event.Skip();
                break;
        }
    }

    // event handlers (these functions should _not_ be virtual)
    void OnQuit(wxCommandEvent& WXUNUSED(event))
    {
        // true is to force the frame to close
        Close(true);
    }

    void OnAbout(wxCommandEvent& WXUNUSED(event))
    {
        wxMessageBox
            (
            wxString::Format
                (
                "Welcome to %s!\n"
                "\n"
                "This is the minimal wxWidgets sample\n"
                "running under %s.",
                wxVERSION_STRING,
                wxGetOsDescription()
                ),
            "About wxWidgets minimal sample",
            wxOK | wxICON_INFORMATION,
            this
            );
    }

private:
    wxImagePanel* mImagePanel;

    typedef std::list< wxFileName > FileList;
    FileList mFiles;
    FileList::iterator mCurFile;
};


// Define a new application type, each program should derive a class from wxApp
class MyApp : public wxApp
{
public:
    MyApp() : mInitialPath( wxGetCwd() ) { }

    virtual void OnInitCmdLine( wxCmdLineParser& parser )
    {
        wxApp::OnInitCmdLine( parser );
        parser.AddParam
            (
            "File or directory to display",
            wxCMD_LINE_VAL_STRING,
            wxCMD_LINE_PARAM_OPTIONAL
            );
    }

    virtual bool OnCmdLineParsed( wxCmdLineParser& parser )
    {
        if( parser.GetParamCount() == 1 )
        {
            // single argument, either a file or a directory
            mInitialPath = parser.GetParam( 0 );
        }

        return wxApp::OnCmdLineParsed( parser );
    }

    // this one is called on application startup and is a good place for the app
    // initialization (doing it here and not in the ctor allows to have an error
    // return: if OnInit() returns false, the application terminates)
    // 'Main program' equivalent: the program execution "starts" here
    virtual bool OnInit()
    {
        // call the base class initialization method, currently it only parses a
        // few common command-line options but it could be do more in the future
        if ( !wxApp::OnInit() )
            return false;

        // handle ALL the images!
        wxInitAllImageHandlers();

        // create the main application window
        MyFrame *frame = new MyFrame( "QndView", mInitialPath );

        // and show it (the frames, unlike simple controls, are not shown when
        // created initially)
        frame->Show(true);

        // success: wxApp::OnRun() will be called which will enter the main message
        // loop and the application will run. If we returned false here, the
        // application would exit immediately.
        return true;
    }

    wxString mInitialPath;
};

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also implements the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
IMPLEMENT_APP(MyApp)