inherited BitmapEditFrame: TBitmapEditFrame
  Width = 438
  Height = 354
  OnResize = FrameResize
  ExplicitWidth = 438
  ExplicitHeight = 354
  object Splitter1: TSplitter
    Left = 249
    Top = 0
    Width = 4
    Height = 354
    Align = alRight
    ExplicitLeft = 233
  end
  object LeftPanel: TGroupBox
    Left = 0
    Top = 0
    Width = 249
    Height = 354
    Align = alClient
    Caption = 'Graph'
    TabOrder = 0
    object ScrollBox1: TScrollBox
      Left = 2
      Top = 15
      Width = 245
      Height = 337
      Align = alClient
      BevelInner = bvNone
      BevelOuter = bvNone
      BorderStyle = bsNone
      PopupMenu = PopupMenu1
      TabOrder = 0
      object Image: TImage
        Left = 0
        Top = 0
        Width = 161
        Height = 270
        PopupMenu = PopupMenu1
        OnMouseDown = ImageMouseDown
        OnMouseMove = ImageMouseMove
        OnMouseUp = ImageMouseUp
      end
    end
  end
  object RightPanel: TGroupBox
    Left = 253
    Top = 0
    Width = 185
    Height = 354
    Align = alRight
    Caption = 'Preview'
    TabOrder = 1
    object PreviewPanel: TPanel
      Left = 2
      Top = 15
      Width = 181
      Height = 319
      Align = alClient
      BevelOuter = bvNone
      TabOrder = 0
    end
    object DisablePreviewCheckBox: TCheckBox
      Left = 2
      Top = 334
      Width = 181
      Height = 18
      Align = alBottom
      Caption = 'Hide preview'
      TabOrder = 1
      OnClick = DisablePreviewCheckBoxClick
    end
  end
  object PopupMenu1: TPopupMenu
    OnPopup = PopupMenu1Popup
    Left = 63
    Top = 169
    object AddMenuItem: TMenuItem
      Caption = 'Add'
    end
    object DeleteMenuItem: TMenuItem
      Caption = 'Delete'
      OnClick = DeleteMenuItemClick
    end
  end
end