#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include "ImagePanel.h"

using namespace std;


// IDs for the controls and the menu commands
enum
{
    // menu items
    Minimal_Quit = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    Minimal_About = wxID_ABOUT
};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString& title)
        : wxFrame( NULL, wxID_ANY, title )
        , mImagePanel( new wxImagePanel( this ) )
    {
        // create a menu bar
        wxMenu *fileMenu = new wxMenu;

        // the "About" item should be in the help menu
        wxMenu *helpMenu = new wxMenu;
        helpMenu->Append(Minimal_About, "&About\tF1", "Show about dialog");

        fileMenu->Append(Minimal_Quit, "E&xit\tAlt-X", "Quit this program");

        // now append the freshly created menu to the menu bar...
        wxMenuBar *menuBar = new wxMenuBar();
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(helpMenu, "&Help");

        // ... and attach this menu bar to the frame
        SetMenuBar(menuBar);

        // create a status bar just for fun (by default with 1 pane only)
        CreateStatusBar(2);
        SetStatusText("Welcome to wxWidgets!");

        mImage = new wxImage();
        string fileName( "test.png" );
        bool success = false;
        {
            // bug workaround
            // http://trac.wxwidgets.org/ticket/15331
            wxLogNull logNo;
            success = mImage->LoadFile( fileName );
        }

        mImagePanel->SetImage( mImage );

        Bind( wxEVT_CHAR_HOOK, &MyFrame::OnCharHook, this );
    }

    void OnCharHook( wxKeyEvent& event )
    {
        switch( event.GetKeyCode() )
        {
            case 'F':
                // toggle fullscreen
                ShowFullScreen( !IsFullScreen() );
                return;
                break;
            default:
                break;
        }
        event.Skip();
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
    wxSharedPtr< wxImage > mImage;

    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();
};

// the event tables connect the wxWidgets events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(Minimal_Quit,  MyFrame::OnQuit)
    EVT_MENU(Minimal_About, MyFrame::OnAbout)
wxEND_EVENT_TABLE()


// Define a new application type, each program should derive a class from wxApp
class MyApp : public wxApp
{
public:
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
        MyFrame *frame = new MyFrame("Minimal wxWidgets App");

        // and show it (the frames, unlike simple controls, are not shown when
        // created initially)
        frame->Show(true);

        // success: wxApp::OnRun() will be called which will enter the main message
        // loop and the application will run. If we returned false here, the
        // application would exit immediately.
        return true;
    }
};

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also implements the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
IMPLEMENT_APP(MyApp)