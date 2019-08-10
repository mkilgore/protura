
(defun list (x . y)
  (cons x y)
  )

(defmacro progn (expr . extra)
  (list
    (cons 'lambda (cons () (cons expr extra))
      )))

(defun range (r1 r2)
  (cons r1
    (if (< r1 r2)
       (range (+ r1 1) r2)
       ()
       )))

(defmacro for (lst_init iter . body)
  (list
    (list 'lambda '(lst)
      (list 'while 'lst
        (list
          (cons 'lambda (cons (list iter) body))
          '(car lst)
          )

        (list 'setq 'lst '(cdr lst))
        ))
    lst_init
    ))

(defun add-range (r1 r2)
  ((lambda (sum)
    (for (range r1 r2) i
      (setq sum (+ sum i))
      )
    sum
    ) 0
   ))

(add-range 1 100)

(+ (add-range 1 50) (add-range 10 20))
(- (add-range 1 50) (add-range 10 20))

