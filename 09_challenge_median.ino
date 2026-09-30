// Arduino pin assignment
#define PIN_LED  9
#define PIN_TRIG 12
#define PIN_ECHO 13

// configurable parameters
#define SND_VEL 346.0     // sound velocity at 24 celsius degree (unit: m/sec)
#define INTERVAL 25       // sampling interval (unit: msec)
#define PULSE_DURATION 10 // ultra-sound Pulse Duration (unit: usec)
#define _DIST_MIN 100     // minimum distance to be measured (unit: mm)
#define _DIST_MAX 300     // maximum distance to be measured (unit: mm)

#define TIMEOUT ((INTERVAL / 2) * 1000.0) // maximum echo waiting time (unit: usec)
#define SCALE (0.001 * 0.5 * SND_VEL)     // coefficent to convert duration to distance

#define _EMA_ALPHA 0.5    // EMA weight of new sample (range: 0 to 1)

#define _MEDIAN_N 3       // 중위수 필터 샘플 수: 3, 10, 30으로 바꿔가며 테스트

// global variables
unsigned long last_sampling_time;   // unit: msec
float dist_ema;                     // EMA distance
float dist_median;                  // median distance

// 최근 N개 샘플을 유지하는 원형 버퍼
float samples[_MEDIAN_N];
int sample_idx = 0;    // 다음에 쓸 위치
int sample_cnt = 0;    // 현재까지 채워진 샘플 수 (최대 N)

void setup() {
  // initialize GPIO pins
  pinMode(PIN_LED,OUTPUT);
  pinMode(PIN_TRIG,OUTPUT);
  pinMode(PIN_ECHO,INPUT);
  digitalWrite(PIN_TRIG, LOW);

  // initialize serial port
  Serial.begin(57600);
}

void loop() {
  float dist_raw;

  // wait until next sampling time.
  if (millis() < last_sampling_time + INTERVAL)
    return;

  // get a distance reading from the USS
  dist_raw = USS_measure(PIN_TRIG,PIN_ECHO);

  // 직전 유효값 적용 코드 없이 raw 값을 그대로 중위수 필터에 넣는다
  add_sample(dist_raw);
  dist_median = get_median();

  // EMA (raw 기준)
  dist_ema = _EMA_ALPHA * dist_raw + (1.0 - _EMA_ALPHA) * dist_ema;

  // output the read value to the serial port
  Serial.print("Min:");      Serial.print(_DIST_MIN);
  Serial.print(",raw:");     Serial.print(dist_raw);
  Serial.print(",ema:");     Serial.print(dist_ema);
  Serial.print(",median:");  Serial.print(dist_median);
  Serial.print(",Max:");     Serial.print(_DIST_MAX);
  Serial.println("");

  // LED: 중위수 값이 범위 안이면 ON
  if ((dist_median < _DIST_MIN) || (dist_median > _DIST_MAX))
    digitalWrite(PIN_LED, 1);       // LED OFF
  else
    digitalWrite(PIN_LED, 0);       // LED ON

  // update last sampling time
  last_sampling_time += INTERVAL;
}

// 원형 버퍼에 새 샘플 추가. 버퍼가 차면 가장 오래된 샘플을 덮어쓴다.
void add_sample(float value)
{
  samples[sample_idx] = value;
  sample_idx = (sample_idx + 1) % _MEDIAN_N;
  if (sample_cnt < _MEDIAN_N)
    sample_cnt++;
}

// 버퍼의 샘플을 복사해 정렬한 뒤 중위수를 반환한다.
// 원본 버퍼는 시간 순서를 유지해야 하므로 복사본을 정렬한다.
float get_median()
{
  float sorted[_MEDIAN_N];
  int i, j;

  for (i = 0; i < sample_cnt; i++)
    sorted[i] = samples[i];

  // 삽입 정렬 (N이 작으므로 충분)
  for (i = 1; i < sample_cnt; i++) {
    float key = sorted[i];
    j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }

  // 샘플 수가 홀수면 가운데 값, 짝수면 가운데 두 값의 평균
  if (sample_cnt % 2 == 1)
    return sorted[sample_cnt / 2];
  else
    return (sorted[sample_cnt / 2 - 1] + sorted[sample_cnt / 2]) / 2.0;
}

// get a distance reading from USS. return value is in millimeter.
float USS_measure(int TRIG, int ECHO)
{
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(PULSE_DURATION);
  digitalWrite(TRIG, LOW);

  return pulseIn(ECHO, HIGH, TIMEOUT) * SCALE; // unit: mm
}
