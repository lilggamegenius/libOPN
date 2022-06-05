VERSION 5.00
Begin VB.Form Form1 
   BorderStyle     =   1  'Fest Einfach
   Caption         =   "OPN DLL Test"
   ClientHeight    =   3030
   ClientLeft      =   45
   ClientTop       =   375
   ClientWidth     =   4560
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   202
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   304
   StartUpPosition =   3  'Windows-Standard
   Begin VB.TextBox Text2 
      Alignment       =   1  'Rechts
      Height          =   285
      Left            =   1680
      TabIndex        =   3
      Text            =   "100"
      Top             =   1635
      Width           =   1215
   End
   Begin VB.Timer Timer1 
      Interval        =   500
      Left            =   3000
      Top             =   2520
   End
   Begin VB.TextBox Text1 
      Alignment       =   1  'Rechts
      Height          =   285
      Left            =   1680
      TabIndex        =   1
      Text            =   "0"
      Top             =   1275
      Width           =   1215
   End
   Begin VB.Label Label2 
      Caption         =   "DAC Volume:"
      Height          =   255
      Left            =   240
      TabIndex        =   2
      Top             =   1680
      Width           =   1335
   End
   Begin VB.Label Label1 
      Caption         =   "DAC Frequency:"
      Height          =   255
      Left            =   240
      TabIndex        =   0
      Top             =   1320
      Width           =   1335
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Declare Sub SetOPNOptions Lib "OPN_DLL" (ByVal OutSmplRate As Long, ByVal ResmplMode As Byte, _
    ByVal ChipSmplMode As Byte, ByVal ChipSmplRate As Long)
Private Declare Function OpenOPNDriver Lib "OPN_DLL" (ByVal Chips As Byte) As Byte
Private Declare Sub CloseOPNDriver Lib "OPN_DLL" ()

Private Declare Sub OPN_Write Lib "OPN_DLL" (ByVal ChipID As Byte, ByVal Register As Integer, ByVal Data As Byte)
Private Declare Sub OPN_Mute Lib "OPN_DLL" (ByVal ChipID As Byte, ByVal MuteMask As Byte)

Private Declare Sub PlayDACSample Lib "OPN_DLL" (ByVal ChipID As Byte, ByVal DataSize As Long, _
    ByRef Data As Any, ByVal SmplFreq As Long)
Private Declare Sub SetDACFrequency Lib "OPN_DLL" (ByVal ChipID As Byte, ByVal SmplFreq As Long)
Private Declare Sub SetDACVolume Lib "OPN_DLL" (ByVal ChipID As Byte, ByVal Volume As Integer)


' Resampling Modes
Const OPT_RSMPL_HIGH As Byte = &H0      ' high quality linear resampling [default]
Const OPT_RSMPL_LQ_DOWN As Byte = &H1   ' low quality downsampling, high quality upsampling
Const OPT_RSMPL_LOW As Byte = &H2       ' low quality resampling

' Chip Sample Rate Modes
Const OPT_CSMPL_NATIVE As Byte = &H0    ' native chip sample rate [default]
Const OPT_CSMPL_HIGHEST As Byte = &H1   ' highest sample rate (native or custom)
Const OPT_CSMPL_CUSTOM As Byte = &H2    ' custom sample rate

Private Type DRUM_SOUND
    Size As Long
    Data() As Byte
End Type

Const DRUM_COUNT As Byte = 2
Dim DrumLib(0 To DRUM_COUNT - 1) As DRUM_SOUND
Dim NextDrum As Byte

Private Sub Form_Load()

    Dim RetVal As Byte
    
    RetVal = OpenOPNDriver(1)
    If RetVal > &H0 Then
        MsgBox "Error creating YM2612 chip!" & vbNewLine & "Errorcode: " & Hex$(RetVal), vbCritical
        End
    End If
    
    ' Load the drums (ripped from Wolfteam games on the X68000)
    Call LoadDrumSound("00_BassDrum.raw", DrumLib(0))
    Call LoadDrumSound("01_Snare.raw", DrumLib(1))
    
    Text1.Text = "15625"
    Call Text1_KeyPress(&HD)
    
    Call OPN_Write(0, &H2B, &H80)
    NextDrum = 0

End Sub

Private Sub LoadDrumSound(ByVal FileName As String, ByRef DrumSnd As DRUM_SOUND)

    Open FileName For Input As #1
    Close #1
    Open FileName For Binary Access Read As #1
        DrumSnd.Size = LOF(1)
        ReDim DrumSnd.Data(&H0 To DrumSnd.Size - 1)
        Get #1, 1, DrumSnd.Data()
    Close #1

End Sub

Private Sub Form_Unload(Cancel As Integer)

    Call CloseOPNDriver

End Sub

Private Sub Text1_KeyPress(KeyAscii As Integer)

    If KeyAscii = &HD Then
        ' Enter
        Dim FreqVal As Long
        
        KeyAscii = &H0
        
        FreqVal = CLng(Text1.Text)
        Call SetDACFrequency(0, FreqVal)
    End If

End Sub

Private Sub Text2_KeyPress(KeyAscii As Integer)

    If KeyAscii = &HD Then
        ' Enter
        Dim VolVal As Long
        
        KeyAscii = &H0
        
        VolVal = CLng(Text2.Text)
        Call SetDACVolume(0, VolVal / 100 * &H100)
    End If

End Sub

Private Sub Timer1_Timer()

    With DrumLib(NextDrum)
        Call PlayDACSample(0, .Size, .Data(0), 0)
    End With
    NextDrum = (NextDrum + 1) Mod DRUM_COUNT

End Sub
