#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Input.h>                 // PointerPoint
#include <winrt/Microsoft.UI.Windowing.h>             // AppWindow

#include <winrt/Windows.Data.Json.h>                  // JSON
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>

#include <windows.h>
#include <Shobjidl_core.h>            // IInitializeWithWindow
#include <microsoft.ui.xaml.window.h> // IWindowNative

#include <sstream>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Media;

using Windows::Data::Json::JsonArray;
using Windows::Data::Json::JsonObject;
using Windows::Data::Json::JsonValue;
using Windows::Foundation::IAsyncAction;

namespace winrt::Q2_2::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // 창 크기 1024x800
        if (auto aw = this->AppWindow())
        {
            aw.Resize({ 1024, 800 });
        }

        // UI 초기 동기화
        if (auto cp = this->PenColorPicker())
        {
            cp.Color(m_currentColor);
        }
        UpdateColorPreview();

        if (auto tb = this->ThicknessValueText())
        {
            tb.Text(L"2.0 px");
        }
    }
    int32_t MainWindow::MyProperty() { throw hresult_not_implemented(); }
    void MainWindow::MyProperty(int32_t) { throw hresult_not_implemented(); }
    // ---- Win32 HWND로 FilePicker 초기화 (WinApp SDK 버전 무관) ----
    void MainWindow::InitializePickerWithWindow(Windows::Foundation::IInspectable const& picker)
    {
        // this(Window) -> IWindowNative -> HWND
        if (auto native = this->try_as<::IWindowNative>())
        {
            HWND hwnd{};
            if (SUCCEEDED(native->get_WindowHandle(&hwnd)) && hwnd)
            {
                if (auto init = picker.try_as<::IInitializeWithWindow>())
                {
                    (void)init->Initialize(hwnd);
                }
            }
        }
    }

    // ==== 오류 다이얼로그 ====
    IAsyncAction MainWindow::ShowErrorDialog(hstring const& message)
    {
        ContentDialog dlg{};
        dlg.XamlRoot(this->Content().XamlRoot());  // WinUI 3: XamlRoot 반드시 설정
        dlg.Title(box_value(L"오류"));
        dlg.Content(box_value(message));
        dlg.CloseButtonText(L"확인");
        co_await dlg.ShowAsync();
    }

    // ==== ColorPicker: 드래그 중 연속 호출 → 미리보기만 갱신 ====
    void MainWindow::PenColorPicker_ColorChanged(IInspectable const&,
        Microsoft::UI::Xaml::Controls::ColorChangedEventArgs const& e)
    {
        m_pendingColor = e.NewColor();
        UpdateColorPreview();
    }

    // ==== [적용] 버튼: 이때만 실제 펜 색 확정 ====
    void MainWindow::ApplyColorButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_currentColor = m_pendingColor;

        if (auto cp = this->PenColorPicker())
            cp.Color(m_currentColor);
        UpdateColorPreview();

        if (auto f = this->ColorFlyout())
            f.Hide();
    }

    void MainWindow::CloseColorFlyout_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (auto f = this->ColorFlyout())
            f.Hide();
    }

    // ==== 굵기 ====
    void MainWindow::ThicknessSlider_ValueChanged(IInspectable const&,
        Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        m_currentThickness = e.NewValue();

        if (auto tb = this->ThicknessValueText())
        {
            wchar_t buf[32]{};
            swprintf_s(buf, L"%.1f px", m_currentThickness);
            tb.Text(buf);
        }

        if (m_currentStroke)
            m_currentStroke.StrokeThickness(m_currentThickness);
    }

    // ==== 그리기 시작 ====
    void MainWindow::DrawCanvas_PointerPressed(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        auto canvas = this->DrawCanvas();
        if (!canvas) return;

        auto point = e.GetCurrentPoint(canvas);
        if (!point.Properties().IsLeftButtonPressed()) return;

        m_isDrawing = true;

        Microsoft::UI::Xaml::Shapes::Polyline pl;
        pl.Stroke(Microsoft::UI::Xaml::Media::SolidColorBrush(m_currentColor));
        pl.StrokeThickness(m_currentThickness);
        pl.StrokeLineJoin(Microsoft::UI::Xaml::Media::PenLineJoin::Round);
        pl.StrokeStartLineCap(Microsoft::UI::Xaml::Media::PenLineCap::Round);
        pl.StrokeEndLineCap(Microsoft::UI::Xaml::Media::PenLineCap::Round);

        auto pos = point.Position();
        Windows::Foundation::Point p{ static_cast<float>(pos.X), static_cast<float>(pos.Y) };
        pl.Points().Append(p);

        canvas.Children().Append(pl);
        canvas.CapturePointer(e.Pointer());

        m_currentStroke = pl;

        m_currentData = {};
        m_currentData.color = m_currentColor;
        m_currentData.thickness = m_currentThickness;
        m_currentData.points.push_back(p);

        e.Handled(true);
    }

    // ==== 이동 중 점 추가 ====
    void MainWindow::DrawCanvas_PointerMoved(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (!m_isDrawing || !m_currentStroke) return;

        auto canvas = this->DrawCanvas();
        if (!canvas) return;

        auto point = e.GetCurrentPoint(canvas);
        if (!point.IsInContact()) return;

        auto pos = point.Position();
        Windows::Foundation::Point p{ static_cast<float>(pos.X), static_cast<float>(pos.Y) };
        m_currentStroke.Points().Append(p);
        m_currentData.points.push_back(p);

        e.Handled(true);
    }

    // ==== 끝내기 ====
    void MainWindow::DrawCanvas_PointerReleased(IInspectable const&, PointerRoutedEventArgs const& e)
    {
        if (!m_isDrawing) return;

        if (auto canvas = this->DrawCanvas())
            canvas.ReleasePointerCaptures();

        m_isDrawing = false;

        if (!m_currentData.points.empty())
            m_strokes.push_back(std::move(m_currentData));

        m_currentStroke = nullptr;
        e.Handled(true);
    }

    // ==== 저장 (파일 이름/경로 선택, 안정화) ====
    winrt::fire_and_forget MainWindow::SaveButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        hstring err;   // catch에서 메시지 저장 (catch 안에서 co_await 금지)
        try
        {
            // 문서 JSON 구성
            JsonArray rootArray;
            for (auto const& s : m_strokes)
            {
                JsonObject o;

                JsonArray c;
                c.Append(JsonValue::CreateNumberValue(s.color.A));
                c.Append(JsonValue::CreateNumberValue(s.color.R));
                c.Append(JsonValue::CreateNumberValue(s.color.G));
                c.Append(JsonValue::CreateNumberValue(s.color.B));
                o.Insert(L"color", c);

                o.Insert(L"thickness", JsonValue::CreateNumberValue(s.thickness));

                JsonArray pts;
                for (auto const& pt : s.points)
                {
                    JsonObject jp;
                    jp.Insert(L"x", JsonValue::CreateNumberValue(pt.X));
                    jp.Insert(L"y", JsonValue::CreateNumberValue(pt.Y));
                    pts.Append(jp);
                }
                o.Insert(L"points", pts);

                rootArray.Append(o);
            }

            JsonObject doc;
            doc.Insert(L"version", JsonValue::CreateNumberValue(1));
            doc.Insert(L"strokes", rootArray);

            Windows::Storage::Pickers::FileSavePicker picker;
            InitializePickerWithWindow(picker);
            picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.SuggestedFileName(L"strokes");
            picker.FileTypeChoices().Insert(L"JSON", single_threaded_vector<hstring>({ L".json" }));

            Windows::Storage::StorageFile file = co_await picker.PickSaveFileAsync();
            if (!file) co_return; // 취소

            co_await Windows::Storage::FileIO::WriteTextAsync(file, doc.Stringify());
        }
        catch (winrt::hresult_error const& ex)
        {
            err = L"파일 저장 중 예외가 발생했습니다.\n\n" + ex.message();
        }
        catch (...)
        {
            err = L"파일 저장 중 알 수 없는 오류가 발생했습니다.";
        }

        if (!err.empty())
        {
            co_await ShowErrorDialog(err);
        }
        co_return;
    }

    // ==== 불러오기 (파일 선택, 방어적 파싱) ====
    winrt::fire_and_forget MainWindow::LoadButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        hstring err;   // catch에서 메시지 저장
        try
        {
            Windows::Storage::Pickers::FileOpenPicker picker;
            InitializePickerWithWindow(picker);
            picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.FileTypeFilter().Append(L".json");

            Windows::Storage::StorageFile file = co_await picker.PickSingleFileAsync();
            if (!file) co_return; // 취소

            auto text = co_await Windows::Storage::FileIO::ReadTextAsync(file);

            JsonObject doc;
            if (!JsonObject::TryParse(text, doc))
            {
                err = L"JSON 파싱 실패: 올바른 형식의 파일인지 확인하세요.";
            }
            else
            {
                // strokes 키 확인
                if (!doc.HasKey(L"strokes"))
                {
                    err = L"JSON에 'strokes' 키가 없습니다.";
                }
                else if (doc.Lookup(L"strokes").ValueType() != Windows::Data::Json::JsonValueType::Array)
                {
                    err = L"'strokes' 값이 배열(JSON array)이 아닙니다.";
                }
                else
                {
                    auto arr = doc.Lookup(L"strokes").GetArray();

                    // 모델 갱신
                    m_strokes.clear();

                    for (uint32_t i = 0; i < arr.Size(); ++i)
                    {
                        auto itemVal = arr.GetAt(i);
                        if (itemVal.ValueType() != Windows::Data::Json::JsonValueType::Object)
                            continue;

                        auto o = itemVal.GetObject();
                        StrokeData s{};

                        // color
                        if (o.HasKey(L"color") && o.Lookup(L"color").ValueType() == Windows::Data::Json::JsonValueType::Array)
                        {
                            auto c = o.Lookup(L"color").GetArray();
                            if (c.Size() == 4)
                            {
                                s.color = {
                                    static_cast<uint8_t>(c.GetAt(0).GetNumber()),
                                    static_cast<uint8_t>(c.GetAt(1).GetNumber()),
                                    static_cast<uint8_t>(c.GetAt(2).GetNumber()),
                                    static_cast<uint8_t>(c.GetAt(3).GetNumber())
                                };
                            }
                        }

                        // thickness
                        if (o.HasKey(L"thickness") && o.Lookup(L"thickness").ValueType() == Windows::Data::Json::JsonValueType::Number)
                        {
                            s.thickness = o.Lookup(L"thickness").GetNumber();
                        }

                        // points
                        if (o.HasKey(L"points") && o.Lookup(L"points").ValueType() == Windows::Data::Json::JsonValueType::Array)
                        {
                            auto pts = o.Lookup(L"points").GetArray();
                            s.points.reserve(pts.Size());
                            for (uint32_t k = 0; k < pts.Size(); ++k)
                            {
                                auto pv = pts.GetAt(k);
                                if (pv.ValueType() != Windows::Data::Json::JsonValueType::Object)
                                    continue;

                                auto jp = pv.GetObject();
                                if (jp.HasKey(L"x") && jp.HasKey(L"y") &&
                                    jp.Lookup(L"x").ValueType() == Windows::Data::Json::JsonValueType::Number &&
                                    jp.Lookup(L"y").ValueType() == Windows::Data::Json::JsonValueType::Number)
                                {
                                    float x = static_cast<float>(jp.Lookup(L"x").GetNumber());
                                    float y = static_cast<float>(jp.Lookup(L"y").GetNumber());
                                    s.points.push_back({ x, y });
                                }
                            }
                        }

                        if (!s.points.empty())
                            m_strokes.push_back(std::move(s));
                    }

                    // UI 갱신
                    RedrawFromModel();
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            err = L"파일 읽기 중 예외가 발생했습니다.\n\n" + ex.message();
        }
        catch (...)
        {
            err = L"파일 읽기 중 알 수 없는 오류가 발생했습니다.";
        }

        if (!err.empty())
        {
            co_await ShowErrorDialog(err);
        }
        co_return;
    }

    // ==== 색 미리보기 칩 ====
    void MainWindow::UpdateColorPreview()
    {
        if (auto chip = this->ColorPreview())
        {
            chip.Fill(Microsoft::UI::Xaml::Media::SolidColorBrush(m_pendingColor));
        }
    }

    // ==== 모델 → 캔버스 재구성 ====
    void MainWindow::RedrawFromModel()
    {
        auto canvas = this->DrawCanvas();
        if (!canvas) return;

        canvas.Children().Clear();

        for (auto const& s : m_strokes)
        {
            if (s.points.empty()) continue;

            Microsoft::UI::Xaml::Shapes::Polyline pl;
            pl.Stroke(Microsoft::UI::Xaml::Media::SolidColorBrush(s.color));
            pl.StrokeThickness(s.thickness);
            pl.StrokeLineJoin(Microsoft::UI::Xaml::Media::PenLineJoin::Round);
            pl.StrokeStartLineCap(Microsoft::UI::Xaml::Media::PenLineCap::Round);
            pl.StrokeEndLineCap(Microsoft::UI::Xaml::Media::PenLineCap::Round);

            for (auto const& pt : s.points)
            {
                pl.Points().Append(pt);
            }

            canvas.Children().Append(pl);
        }
    }
}
void winrt::Q2_2::implementation::MainWindow::DeleteButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
{
    if (auto canvas = this->DrawCanvas())
    {
        canvas.ReleasePointerCaptures();
        canvas.Children().Clear();
    }
    m_isDrawing = false;
    m_currentStroke = nullptr;
    m_currentData = {};
    m_strokes.clear();
}
