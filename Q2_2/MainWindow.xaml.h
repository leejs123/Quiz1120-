#pragma once
#include "MainWindow.g.h"

#include <vector>

#include <winrt/Windows.UI.h>                                // Color
#include <winrt/Windows.Foundation.h>                        // Point, IInspectable
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>     // Slider ValueChanged args
#include <winrt/Microsoft.UI.Xaml.Media.h>

// JSON
#include <winrt/Windows.Data.Json.h>

// 파일 피커 (Win32 HWND 초기화용 인터페이스)
#include <windows.h>
#include <Shobjidl_core.h>            // IInitializeWithWindow
#include <microsoft.ui.xaml.window.h> // IWindowNative (WinUI 3 Window → HWND)

namespace winrt::Q2_2::implementation
{
    struct StrokeData
    {
        winrt::Windows::UI::Color color{};
        double thickness{ 2.0 };
        std::vector<winrt::Windows::Foundation::Point> points{};
    };

    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        int32_t MyProperty();
        void MyProperty(int32_t value);
        // 그리기(포인터) 이벤트
        void DrawCanvas_PointerPressed(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void DrawCanvas_PointerMoved(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void DrawCanvas_PointerReleased(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        // 툴바 이벤트
        void PenColorPicker_ColorChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs const& e);
        void ApplyColorButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void CloseColorFlyout_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void ThicknessSlider_ValueChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);

        // 저장/불러오기
        winrt::fire_and_forget SaveButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        winrt::fire_and_forget LoadButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        // 현재 스트로크 핸들 및 데이터
        winrt::Microsoft::UI::Xaml::Shapes::Polyline m_currentStroke{ nullptr };
        StrokeData m_currentData{};
        bool m_isDrawing{ false };

        // 전역 펜 상태(다음 스트로크에 적용)
        winrt::Windows::UI::Color m_currentColor{ 255, 0, 0, 0 };
        winrt::Windows::UI::Color m_pendingColor{ 255, 0, 0, 0 };
        double m_currentThickness{ 2.0 };

        // 완료된 스트로크(문서 모델)
        std::vector<StrokeData> m_strokes;

        // 유틸
        void UpdateColorPreview();
        void RedrawFromModel();
        void InitializePickerWithWindow(winrt::Windows::Foundation::IInspectable const& picker);

        // 오류 메시지 다이얼로그
        winrt::Windows::Foundation::IAsyncAction ShowErrorDialog(winrt::hstring const& message);
    public:
        void DeleteButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::Q2_2::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
