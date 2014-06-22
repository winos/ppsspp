#ifndef QTMAIN_H
#define QTMAIN_H

#include <QTouchEvent>
#include <QMouseEvent>
#include <QInputDialog>
#include "gfx_es2/glsl_program.h"
#include <QGLWidget>

#include <QAudioOutput>
#include <QAudioFormat>
#if defined(MOBILE_DEVICE) && !defined(MAEMO)
#include <QAccelerometer>
#if QT_VERSION < 0x50000
QTM_USE_NAMESPACE
#endif
#endif

#include "base/display.h"
#include "base/logging.h"
#include "base/timeutil.h"
#include "file/zip_read.h"
#include "gfx_es2/gl_state.h"
#include "input/input_state.h"
#include "input/keycodes.h"
#include "base/NativeApp.h"
#include "net/resolve.h"
#include "base/NKCodeFromQt.h"

// Bad: PPSSPP includes from native
#include "Core/System.h"
#include "Core/Core.h"
#include "Core/Config.h"

// Input
void SimulateGamepad(InputState *input);

//GUI
class MainUI : public QGLWidget
{
	Q_OBJECT
public:
	explicit MainUI(QWidget *parent = 0):
		QGLWidget(parent)
	{
		setAttribute(Qt::WA_AcceptTouchEvents);
#if QT_VERSION < 0x50000
		setAttribute(Qt::WA_LockLandscapeOrientation);
#endif
#if defined(MOBILE_DEVICE) && !defined(MAEMO)
		acc = new QAccelerometer(this);
		acc->start();
#endif
		setFocus();
		setFocusPolicy(Qt::StrongFocus);
		startTimer(16);
	}
	~MainUI() {
#if defined(MOBILE_DEVICE) && !defined(MAEMO)
		delete acc;
#endif
		NativeShutdownGraphics();
	}

public slots:
	QString InputBoxGetQString(QString title, QString defaultValue) {
		bool ok;
		QString text = QInputDialog::getText(this, title, title, QLineEdit::Normal, defaultValue, &ok);
		if (!ok)
			text = QString();
		return text;
	}

signals:
	void doubleClick();
	void newFrame();

protected:
	void resizeEvent(QResizeEvent * e)
	{
		pixel_xres = e->size().width();
		pixel_yres = e->size().height();
		dp_xres = pixel_xres * g_dpi_scale;
		dp_yres = pixel_yres * g_dpi_scale;
		PSP_CoreParameter().pixelWidth = pixel_xres;
		PSP_CoreParameter().pixelHeight = pixel_yres;
		UpdateScreenScale(pixel_xres, pixel_yres);
	}

	void timerEvent(QTimerEvent *) {
		updateGL();
		emit newFrame();
	}
	bool event(QEvent *e)
	{
		TouchInput input;
		QList<QTouchEvent::TouchPoint> touchPoints;
		switch(e->type())
		{
		case QEvent::TouchBegin:
		case QEvent::TouchUpdate:
		case QEvent::TouchEnd:
			touchPoints = static_cast<QTouchEvent *>(e)->touchPoints();
			foreach (const QTouchEvent::TouchPoint &touchPoint, touchPoints) {
				switch (touchPoint.state()) {
				case Qt::TouchPointStationary:
					break;
				case Qt::TouchPointPressed:
				case Qt::TouchPointReleased:
					input_state.pointer_down[touchPoint.id()] = (touchPoint.state() == Qt::TouchPointPressed);
					input_state.pointer_x[touchPoint.id()] = touchPoint.pos().x() * g_dpi_scale;
					input_state.pointer_y[touchPoint.id()] = touchPoint.pos().y() * g_dpi_scale;

					input.x = touchPoint.pos().x() * g_dpi_scale;
					input.y = touchPoint.pos().y() * g_dpi_scale;
					input.flags = (touchPoint.state() == Qt::TouchPointPressed) ? TOUCH_DOWN : TOUCH_UP;
					input.id = touchPoint.id();
					NativeTouch(input);
					break;
				case Qt::TouchPointMoved:
					input_state.pointer_x[touchPoint.id()] = touchPoint.pos().x() * g_dpi_scale;
					input_state.pointer_y[touchPoint.id()] = touchPoint.pos().y() * g_dpi_scale;

					input.x = touchPoint.pos().x() * g_dpi_scale;
					input.y = touchPoint.pos().y() * g_dpi_scale;
					input.flags = TOUCH_MOVE;
					input.id = touchPoint.id();
					NativeTouch(input);
					break;
				default:
					break;
				}
			}
			break;
		case QEvent::MouseButtonDblClick:
			if (!g_Config.bShowTouchControls || GetUIState() != UISTATE_INGAME)
				emit doubleClick();
			break;
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
			input_state.pointer_down[0] = (e->type() == QEvent::MouseButtonPress);
			input_state.pointer_x[0] = ((QMouseEvent*)e)->pos().x() * g_dpi_scale;
			input_state.pointer_y[0] = ((QMouseEvent*)e)->pos().y() * g_dpi_scale;

			input.x = ((QMouseEvent*)e)->pos().x() * g_dpi_scale;
			input.y = ((QMouseEvent*)e)->pos().y() * g_dpi_scale;
			input.flags = (e->type() == QEvent::MouseButtonPress) ? TOUCH_DOWN : TOUCH_UP;
			input.id = 0;
			NativeTouch(input);
			break;
		case QEvent::MouseMove:
			input_state.pointer_x[0] = ((QMouseEvent*)e)->pos().x() * g_dpi_scale;
			input_state.pointer_y[0] = ((QMouseEvent*)e)->pos().y() * g_dpi_scale;

			input.x = ((QMouseEvent*)e)->pos().x() * g_dpi_scale;
			input.y = ((QMouseEvent*)e)->pos().y() * g_dpi_scale;
			input.flags = TOUCH_MOVE;
			input.id = 0;
			NativeTouch(input);
			break;
		case QEvent::Wheel:
			NativeKey(KeyInput(DEVICE_ID_MOUSE, ((QWheelEvent*)e)->delta()<0 ? NKCODE_EXT_MOUSEWHEEL_DOWN : NKCODE_EXT_MOUSEWHEEL_UP, KEY_DOWN));
			break;
		case QEvent::KeyPress:
			NativeKey(KeyInput(DEVICE_ID_KEYBOARD, KeyMapRawQttoNative.find(((QKeyEvent*)e)->key())->second, KEY_DOWN));
			break;
		case QEvent::KeyRelease:
			NativeKey(KeyInput(DEVICE_ID_KEYBOARD, KeyMapRawQttoNative.find(((QKeyEvent*)e)->key())->second, KEY_UP));
			break;
		default:
			return QWidget::event(e);
		}
		e->accept();
		return true;
	}

	void initializeGL()
	{
#ifndef USING_GLES2
		glewInit();
#endif
		NativeInitGraphics();
	}

	void paintGL()
	{
		updateAccelerometer();
		UpdateInputState(&input_state);
		NativeUpdate(input_state);
		NativeRender();
		EndInputState(&input_state);
		time_update();
	}

	void updateAccelerometer()
	{
#if defined(MOBILE_DEVICE) && !defined(MAEMO)
		// TODO: Toggle it depending on whether it is enabled
		QAccelerometerReading *reading = acc->reading();
		if (reading) {
			input_state.acc.x = reading->x();
			input_state.acc.y = reading->y();
			input_state.acc.z = reading->z();
			AxisInput axis;
			axis.deviceId = DEVICE_ID_ACCELEROMETER;
			axis.flags = 0;

			axis.axisId = JOYSTICK_AXIS_ACCELEROMETER_X;
			axis.value = input_state.acc.x;
			NativeAxis(axis);

			axis.axisId = JOYSTICK_AXIS_ACCELEROMETER_Y;
			axis.value = input_state.acc.y;
			NativeAxis(axis);

			axis.axisId = JOYSTICK_AXIS_ACCELEROMETER_Z;
			axis.value = input_state.acc.z;
			NativeAxis(axis);
		}
#endif
	}

private:
	InputState input_state;
#if defined(MOBILE_DEVICE) && !defined(MAEMO)
	QAccelerometer* acc;
#endif
};

static MainUI* emugl = NULL;

// Audio
#define AUDIO_FREQ 44100
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES 2048
#define AUDIO_SAMPLESIZE 16
class MainAudio: public QObject
{
	Q_OBJECT
public:
	MainAudio() {
	}
	~MainAudio() {
		if (feed != NULL) {
			killTimer(timer);
			feed->close();
		}
		if (output) {
			output->stop();
			delete output;
		}
		if (mixbuf)
			free(mixbuf);
	}
public slots:
	void run() {
		QAudioFormat fmt;
		fmt.setSampleRate(AUDIO_FREQ);
		fmt.setCodec("audio/pcm");
		fmt.setChannelCount(AUDIO_CHANNELS);
		fmt.setSampleSize(AUDIO_SAMPLESIZE);
		fmt.setByteOrder(QAudioFormat::LittleEndian);
		fmt.setSampleType(QAudioFormat::SignedInt);
		mixlen = 5*2*AUDIO_CHANNELS*AUDIO_SAMPLES;
		mixbuf = (char*)malloc(mixlen);
		output = new QAudioOutput(fmt);
		output->setBufferSize(mixlen);
		feed = output->start();
		if (feed != NULL)
			timer = startTimer(1000*AUDIO_SAMPLES / AUDIO_FREQ);
	}

protected:
	void timerEvent(QTimerEvent *) {
		memset(mixbuf, 0, mixlen);
		size_t frames = NativeMix((short *)mixbuf, 5*AUDIO_SAMPLES);
		if (frames > 0)
			feed->write(mixbuf, sizeof(short) * 2 * frames);
	}
private:
	QIODevice* feed;
	QAudioOutput* output;
	int mixlen;
	char* mixbuf;
	int timer;
};

#endif

