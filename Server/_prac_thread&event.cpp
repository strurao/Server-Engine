#include "pch.h"
#include <iostream>
#include <thread> // c++ 표준 라이브러리
#include <vector>
#include "windows.h"
#include <atomic> // 원자적! 공유자원과 연관이 있다. 그러나 교통정리를 하느라 훨씬 느리다
#include <Windows.h>
#include <mutex> // Lock
#include <queue>
using namespace std;


/*
영역    주의   사유                                 예

Code    X     딱히 수정하지 않음
Stack   X     스레드마다 자신만의 고유스택영역을 사용   지역변수
Heap    O     공유하므로 복잡한 문제가 생길 수 있다    동적배열
Data    O     공유하므로 복잡한 문제가 생길 수 있다    전역/static변수 (코어끼리의 공유자원)
*/


// Mutual Exclusive (상호배타적: 한 번에 하나의 스레드만 실행할 수 있다)
// 만약 3개의 스레드가 접근하려면, 누가 먼저 접근할지 알 수 없다. 가장 빠른 녀석이 자물쇠를 풀고 (임계영역) 화장실을 사용하는 격이다.
// 1,2,3, 중에서 1번이 먼저 갔다가 또다시 1번이 또 들어갈 수도 있다. 아닐 수도 있고. 이것이 경합이다.
// RW Lock같은 특이 케이스를 제외하고 기본적으로 Lock은 한번에 하나이다. 그것이 동기화이다
// Ex) Airplane's Toilet
mutex m;

vector<int> v;


// RAII (Resource Acquisition Is Initialization)
// 일종의 Wrapper Class 이다
// std::lock_guard
/*
template<typename T>
class LockGuard
{
public:
	LockGuard(T& m) : _mutex(m)
	{
		_mutex.lock();
	}

	~LockGuard()
	{
		_mutex.unlock();
	}

private:
	T& _mutex;
};
*/

// 우리만의 락
class Lock
{
public:
	void lock()
	{
		// 아래의 코드는 상호배타성에 어긋난다. 스레드가 틈새로 들어올 수 있다
		/*
		while (_flag)
		{
			// Do Nothing
		}
		_flag = true;
		*/

		// 그래서 사용하는 CAS (Compare-And-Swap)
		bool expected = false;
		bool desired = true;

		// Spin Lock: spin이 계속 뺑뺑이 돌듯 쉬지않고 노크를 한다는 것 (존버멘탈)
		// 장점: 
		// 단점: CPU cost 가 크다. 기내화장실 대기 중인데 앞사람이 술취 후 잠들었다면?
		while (_flag.compare_exchange_strong(expected, desired) == false)
		{
			expected = false;
		}

		// 이것의 수도코드
		/*
		if (_flag == expected)
		{
			_flag = desired;
			return true;
		}
		else
		{
			expected = _flag;
			return false;
		}
		*/
	}

	void unlock()
	{
		_flag = false;
	}

private:
	atomic<bool> _flag = false;
};

Lock m2;

void Push()
{
	for (int i = 0; i < 10000; i++)
	{
		// m.lock();
		// v.push_back(i);
		// m.unlock(); // 자물쇠처럼 잠그고 풀기. 이걸 주석으로 하면 나머지 애들은 영영 대기이다.
		//LockGuard<mutex> lockGuard(m);

		//std::lock_guard<mutex> lockGuard(m); // 이렇게 모두 다 lock을 거는 것은 정답이 아니다

		//std::unique_lock<mutex> uniqueLock(m, std::defer_lock); // lock_guard 와 거의 동일하나, 개발자 수동 잠금
		//uniqueLock.lock();

		std::lock_guard<Lock> lockGuard(m2);

		v.push_back(i);

		if (i == 5000)
		{
			break;
		}
	}
}

void HelloThread(int i)
{
	while (true)
	{
		cout << "Hello Thread " << i << endl;
	}
}


/*int main()
{
	//
	vector<thread> threads;

	// 직원 배치하기
	for (int i = 0; i < 10; i++)
	{
		threads.push_back(thread(HelloThread, i));
	}

	std::thread t(HelloThread, 100); // 프로그램은 하나인데 영혼은 두 개

	cout << "Hello Main " << endl;

	for (thread& t : threads)
	{
		t.get_id(); // 임의의 id 로 구분해볼 수 있겠다
		int a = t.hardware_concurrency(); // 병렬처리 가능한 CPU 코어 개수. 내 PC는 16개이다
		t.join(); // 끝날 때까지 대기한다
	}
	//

	v.reserve(100000);

	thread t1(Push);
	thread t2(Push);

	t1.join();
	t2.join();

	cout << v.size() << endl;
}
*/





////////////////////////////////////////////////






/* 이벤트와 조건 변수 */ 

/mutex m;
queue<int> q;

HANDLE hEvent; // 기내화장실에서 대기할 때, 자리가 비면 알려달라고 승무원님께 부탁드린다 -> 이벤트
// 하나의 정수에 불과하나, 정수가 특정한 커널오브젝트를 지칭한다. 운영체제가 관리하는 오브젝트가 커널 오브젝트.

condition_variable cv; // 조건변수. 커널 오브젝트가 아니라 유저레벨 오브젝트. 이벤트까지 안가도 된다.
/* 
CV 사용법
1) Lock 잡기
2) 공유 변수 값 수정
3) Lock 풀고
4) CV 통해서 통지
*/



// 일감을 찍어내는 쪽
void Producer()
{
	while (true)
	{
		unique_lock<mutex> lock(m);
		q.push(100);

		// ::SetEvent(hEvent); // 신호 바뀜. 여기선 꺼져있다가(빨간불) 이제 켜짐(파란불)
		cv.notify_one(); // 대기중인 스레드가 있다면 딱 1개를 깨운다 (통지)

		this_thread::sleep_for(100ms);
	}
}

// 이렇게 역할분담을 하자~
void Consumer()
{
	while (true)
	{
		// ::WaitForSingleObject(hEvent, INFINITE); // 파란불이 될 때까지 대기하는 중
		// ::ResetEvent(hEvent);

		unique_lock<mutex> lock(m); // 왜 unique를 쓰냐? 중간에 풀어줄 수 있어서.
		cv.wait(lock, []() {return q.empty() == false; }); // predicate(조건)이 묶여있다.
		/*
		1) Lock을 잡으려고 시도 (이미 잡혔으면 스킵)
		2) 조건 확인
			만족O -> 바로 빠져나와 이어서 코드 진행
			만족X -> Lock 풀고 대기로 전환 (잠들어버린다)
		*/
		
		
		if (q.empty() == false) // 이벤트의 단점
		{
			int data = q.front();
			q.pop();
			cout << data << endl;
		}
	}
}




/*
int x = 0;
int y = 0;
int r1 = 0;
int r2 = 0;

bool ready = false; 

void Thread_1()
{
	while (ready == false){}
	y = 1; // Store Y
	r1 = x; // Load X
}

void Thread_2 ()
{
	while (ready == false){}
	x = 1;
	r2 = y;
}
*/

// ctrl k /
//int main()
//{
//	/*
//	커널 오브젝트 EVENT
//	- Usage Count
//	- Signal (파란불) / Non-Signal (빨간불)
//	*/
//
//	// Auto / Manual < bool
//	// 삼국지의 봉화대처럼 껐다 켰다 할 수 있다
//
//	hEvent = ::CreateEvent(NULL/*보안속성*/, FALSE/*bManualReset*/, FALSE/*초기상태*/, NULL);
//
//	thread t1(Producer);
//	thread t2(Consumer);
//
//	t1.join();
//	t2.join();
//
//	::CloseHandle(hEvent);
//	///*
//	int count = 0;
//	while (true)
//	{
//		ready = false;
//		count++;
//
//		x = y = r1 = r2 = 0;
//
//		thread t1(Thread_1);
//		thread t2(Thread_2);
//
//		ready = true;
//
//		t1.join();
//		t2.join();
//
//		if (r1 == 0 && r2 == 0)
//			break;
//	}
//	cout << count << endl;
//	//*/
//}

