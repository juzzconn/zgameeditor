{Copyright (c) 2012 Ville Krumlinde

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.}

//This file is INCLUDEd in ZPlatform.pas

var
  AndroidCurrentMouse : TZPointi;
  AndroidKeys : array[ansichar] of boolean;
  AndroidPath : PAnsiChar;

  AndroidContentPtr : pointer = nil;
  AndroidContentSize : integer;

  AndroidTouchCount : integer = 0;
  AndroidTouch : array[0..10] of record
    Id,X,Y : integer;
  end;

implementation

uses ZOpenGL, ZLog, UnixType, cthreads, jni, AudioPlayer;

function __android_log_write(prio : integer; tag, text : PAnsiChar) : integer; cdecl; external 'liblog.so';
function gettimeofday( val : PTimeVal; tzp : Pointer ) : Integer; cdecl; external 'libc';
function select( n : longint; readfds, writefds, exceptfds : Pointer; var timeout : timeVal ) : longint; cdecl; external 'libc';
function usleep(__useconds:integer):longint;cdecl;external 'libc' name 'usleep';

var
  StartTimeVal : TimeVal;
  KeyMap : array[AnsiChar] of AnsiChar;

procedure Platform_ShutdownScreen;
begin
end;

function Platform_InitScreen(var Width, Height : integer; UseFullScreen : boolean; Title : PAnsiChar; ZApp : pointer) : integer;
begin
  Result := 0;
end;

procedure Platform_SetWindowCaption(Title : PAnsiChar);
begin
end;

procedure Platform_ShowMouse(Visible : boolean);
begin
end;

procedure Platform_SwapBuffers;
begin
end;

procedure Platform_Run(Callback : TRunCallback);
begin
end;


function Platform_IsKeyPressed(C : AnsiChar) : boolean;
begin
  Result := AndroidKeys[ KeyMap[C] ];
end;

function Platform_CommandLine(Switch : PAnsiChar) : boolean;
begin
{  if Switch='s' then
    Result := True  //Disable audio
  else}
    Result := False;
end;

procedure Platform_Error(ErrorMessage : PAnsiChar);
begin
  __android_log_write(6,'ZgeAndroid',ErrorMessage);
end;

function Platform_GetMousePos : TZPointi;
begin
  Result := AndroidCurrentMouse;
end;

procedure Platform_SetMousePos(const X,Y : integer);
begin
end;

function Platform_GetExeFileName : PAnsiChar;
begin
  Result := '';
end;

procedure Platform_WriteFile(FileName : PAnsiChar; Memory : pointer; Size : integer; Append : Boolean);
begin
end;

procedure Platform_ReadFile(FileName : PAnsiChar; var Memory : pointer; var Size : integer; IsRelative : Boolean);
begin
  Memory := nil;
  Size := 0;
end;

function Platform_GenerateFontDisplayLists(Size : integer; FirstChar,LastChar : integer) : integer;
begin
  Result := 0;
end;

function Platform_GetTime : single;
var
  timerTimeVal : TimeVal;
begin
  gettimeofday( @timerTimeVal, nil );
  timerTimeVal.tv_sec := timerTimeVal.tv_sec - StartTimeVal.tv_sec;
  Result := timerTimeVal.tv_sec + (timerTimeVal.tv_usec * 0.000001);
end;

function Platform_GetSystemTime : integer;
var
  timerTimeVal : TimeVal;
begin
  gettimeofday( @timerTimeVal, nil );
  Result := timerTimeVal.tv_sec;
end;

procedure Platform_DesignerSetDC(DC : longword; HWnd : longword);
begin
end;

{$i-}
function Platform_LoadLinkedResource : TZInputStream;
var
  F : file;
  Buf : array[0..511] of AnsiChar;
  Memory : pointer;
  Size : integer;
begin
  Result := nil;
  System.FileMode := 0;

  ZStrCopy(PAnsiChar(@Buf), AndroidPath);
  ZStrCat(PAnsiChar(@Buf), '/zzdc.dat');
  System.Assign(F,PAnsiChar(@Buf));

  System.Reset(F,1);
  if IOResult<>0 then
  begin
    if AndroidContentPtr<>nil then
      Exit( TZInputStream.CreateFromMemory(AndroidContentPtr, AndroidContentSize) );
    Platform_Error('File missing zzdc.dat');
    Exit;
  end;

  Size := System.FileSize(F);

  GetMem(Memory,Size);
  System.BlockRead(F,Memory^,Size);

  System.Close(F);
  Result := TZInputStream.CreateFromMemory(Memory, Size);
end;


const
  MixBufferSamplesSize = SoundBufferSamplesSize*8;  //M�ste vara lika stor som dma-buffern
var
  MixBuffer : packed array[0..(MixBufferSamplesSize*StereoChannels)-1] of TSoundMixUnit;
  AudioTerminated : boolean;

procedure ClipAndCopy(Source : PSoundMixUnit; Dest : PSoundOutputUnit; SampleCount : integer);
var
  I : integer;
  Val : TSoundMixUnit;
begin
  for I := 0 to (SampleCount * StereoChannels)-1 do
  begin
    Val := Source^ div (1 shl (MixPBits-OutputBits));
    if Val>High(TSoundOutputUnit) then
      Dest^ := High(TSoundOutputUnit)
    else if Val<Low(TSoundOutputUnit) then
      Dest^ := Low(TSoundOutputUnit)
    else
      Dest^ := Val;
    Inc(Source);
    Inc(Dest);
  end;
end;

{procedure LogValue(const S : AnsiString; const Value : integer);
begin
  platform_error(pansichar(S + ': ' + ansistring(inttostr(Value))));
end;}

procedure MixAndCopyData(Destination : pointer; len: Integer );
var
  BytesToWrite,SamplesToWrite : integer;
begin
  BytesToWrite := Len;

  SamplesToWrite := BytesToWrite div (SizeOf(TSoundOutputUnit) * StereoChannels);

  if SamplesToWrite*StereoChannels>High(MixBuffer) then
    //test for high limit on buffer
    Exit;

  FillChar(MixBuffer,SamplesToWrite * StereoChannels * SizeOf(TSoundMixUnit),0);

  Platform_EnterMutex(VoicesMutex);
    AudioPlayer.RenderToMixBuffer(@MixBuffer[0],SamplesToWrite);
  Platform_LeaveMutex(VoicesMutex);

  //Flytta paint buffer till dma-soundbuffer
  ClipAndCopy(@MixBuffer[0],Destination,SamplesToWrite);
end;


function AudioCallback(Data : pointer) : longint;
var
  Env : PJNIEnv;
  cAudioTrack : jclass;
  mGetMinBufferSize,mAudioTrack,mPlay,mStop,mRelease,mWrite : JMethodID;
  bufferSizeInBytes : integer;
  track : jobject;
  Buffer : jarray;
  Params : array[0..10] of integer;
  PBuffer : pointer;
  IsCopy : JBoolean;

  function C(const args : array of const) : pointer;
  var
    I : integer;
  begin
     for I := 0 to High(args) do
       Params[I] := args[I].vinteger;
     Result := @Params;
  end;

begin
  //Write to Java AudioTrack.
  //Concept from OpenAL: http://repo.or.cz/w/openal-soft/android.git/blob/HEAD:/Alc/backends/android.c

  Result := 0;

  Platform_error('audiocallback started');

  if CurVM^.AttachCurrentThread(CurVM,@Env,nil)<>0 then
  begin
    Platform_error('error attach thread');
    Exit;
  end;

  env^.PushLocalFrame(env, 2);

  cAudioTrack := env^.FindClass(env, 'android/media/AudioTrack');
  if cAudioTrack=nil then
  begin
    Platform_error('no audiotrack');
    Exit;
  end;

  mGetMinBufferSize := env^.GetStaticMethodID(env, cAudioTrack, 'getMinBufferSize', '(III)I');
  mAudioTrack := env^.GetMethodID(env, cAudioTrack, '<init>', '(IIIIII)V');
  mPlay := env^.GetMethodID(env, cAudioTrack, 'play', '()V');
  mStop := env^.GetMethodID(env, cAudioTrack, 'stop', '()V');
  mRelease := env^.GetMethodID(env, cAudioTrack, 'release', '()V');
  mWrite := env^.GetMethodID(env, cAudioTrack, 'write', '([BII)I');

  bufferSizeInBytes := Env^.CallStaticIntMethodV(env, cAudioTrack, mGetMinBufferSize, C([AudioPlayer.AudioRate, 3, 2]));
  //LogValue('BufferSize',bufferSizeInBytes);

  track := env^.NewObjectV(env, cAudioTrack, mAudioTrack, C([3, AudioPlayer.AudioRate, 3, 2, bufferSizeInBytes, 1]));
  if track=nil then
    Platform_error('could not create track')
  else
  begin
    env^.CallNonvirtualVoidMethod(env, track, cAudioTrack, mPlay);

    buffer := env^.NewByteArray(env, bufferSizeInBytes);

    while not AudioTerminated do
    begin
      PBuffer := env^.GetPrimitiveArrayCritical(env, buffer, IsCopy);
      if PBuffer=nil then
        Platform_error('could not get buffer')
      else
      begin
        MixAndCopyData(PBuffer,bufferSizeInBytes);
        env^.ReleasePrimitiveArrayCritical(env, buffer, pBuffer, 0);
        env^.CallNonvirtualIntMethodV(env, track, cAudioTrack, mWrite, C([buffer, 0, bufferSizeInBytes]));
      end;
    end;

    env^.CallNonvirtualVoidMethod(env, track, cAudioTrack, mStop);
    env^.CallNonvirtualVoidMethod(env, track, cAudioTrack, mRelease);
  end;

  env^.PopLocalFrame(env, nil);
  CurVM^.DetachCurrentThread(CurVM);
end;

procedure Platform_InitAudio;
var
  id : LongWord;
begin
  AudioTerminated := False;
  BeginThread( AudioCallback, nil, id);
end;

procedure Platform_ShutdownAudio;
begin
  AudioTerminated := True;
end;

function Platform_CreateMutex : pointer;
var
  P : PRTLCriticalSection;
begin
  P := GetMem(SizeOf(Pointer));
  InitCriticalSection(P^);
  Result := P;
end;

procedure Platform_FreeMutex(P : pointer);
begin
  DoneCriticalSection(PRTLCriticalSection(P)^);
  FreeMem(P);
end;

procedure Platform_EnterMutex(P : pointer);
begin
  EnterCriticalSection(PRTLCriticalSection(P)^);
end;

procedure Platform_LeaveMutex(P : pointer);
begin
  LeaveCriticalSection(PRTLCriticalSection(P)^);
end;

function Platform_GLLoadProc(const P : PAnsiChar) : pointer;
begin
  Result := nil;
end;

procedure Platform_InitGlobals;
var
  I : integer;
begin
  gettimeofday( @StartTimeVal, nil );

  AndroidCurrentMouse.X := 0;
  AndroidCurrentMouse.Y := 0;

  for I := 0 to 255 do
    KeyMap[chr(I)] := Chr(I);
  for I := ord('A') to ord('Z') do
    KeyMap[chr(I)] := Chr(I + (ord('a')-ord('A')));
end;

function Platform_ShowOptionDialog(App : pointer) : boolean;
begin
  Result := False;
end;

function Platform_GetDisplayRefreshRate : integer;
begin
  Result := 0;
end;

procedure Platform_Sleep(time : integer);
begin
  usleep(time);
end;

function Platform_GetJoystickAxis(JoyId : integer; Axis : integer) : single;
begin
  Result := 0;
end;

function Platform_GetJoystickButton(JoyId : integer; Button : integer) : boolean;
begin
  Result := False;
end;

function Platform_GetJoystickPOV(JoyId : integer) : single;
begin
  Result := -1;
end;


procedure Platform_NetOpen(Url : PAnsiChar; InBrowser : boolean; WebOpen : pointer);
begin
end;

procedure Platform_NetRead(Handle,Buffer : pointer; Size : integer);
begin
end;

function Platform_LoadModule(const Name : PAnsiChar) : integer;
begin
  Result := 0;
end;

function Platform_GetModuleProc(Module : integer; const Name : PAnsiChar) : pointer;
begin
  Result := nil;
end;

function Platform_TouchGetCount : integer;
begin
  Result := AndroidTouchCount;
end;

function Platform_TouchGetPos(const TouchIndex : integer) : TZPointi;
begin
  if TouchIndex>=AndroidTouchCount then
  begin
    Result.X := 0;
    Result.Y := 0;
  end
  else
  begin
    Result.X := AndroidTouch[TouchIndex].X;
    Result.Y := AndroidTouch[TouchIndex].Y;
  end;
end;

function Platform_TouchGetId(const TouchIndex : integer) : integer;
begin
  Result := 0;
  if TouchIndex<AndroidTouchCount then
    Result := AndroidTouch[TouchIndex].Id;
end;

end.